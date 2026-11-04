"""The comparator must reject false agreement and preserve transient frames."""
import copy
import io
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import Mock, patch
import lockstep as ls
import repeat

def frame(role=1):
    return dict(epoch=0,ms=0,scene=0,error=0,ack=0,role=role,masks=[0xf800,0x7e0,31],
        next_ms=20,traces=[],pixels=bytes(ls.PIXEL_BYTES),submissions=[],sha256='same')

class LockstepTests(unittest.TestCase):
    def setUp(self):self.a,self.b=frame(),frame(2)
    def compare(self):return ls.compare_frame(self.a,self.b,None,0,0)['status']

    def test_equal_and_one_pixel_difference(self):
        self.assertEqual(self.compare(),'PASS')
        self.b['pixels']=b'\x01\x00'+self.b['pixels'][2:]
        r=ls.compare_frame(self.a,self.b,None,0,0)
        self.assertEqual((r['status'],r['changed'],r['bbox']),('DIFF_PIXELS',1,[0,0,1,1]))

    def test_both_endpoints_stale_is_not_agreement(self):
        self.a['ms']=self.b['ms']=20
        self.assertEqual(self.compare(),'INVALID_CAPTURE')

    def test_missing_input_acknowledgement(self):
        self.a['ack']=7
        self.assertEqual(ls.compare_frame(self.a,self.b,dict(id=7),0,0)['status'],'DIFF_INPUT')

    def test_rng_extra_draw_and_identical_bad_algorithm(self):
        state=(100000*1103515245+12345)&0xffffffff
        trace=[2,0,0,0x5d2c,100000,state,(state>>16)&32767,0]
        self.a['traces']=[trace]
        self.assertEqual(self.compare(),'DIFF_RNG')
        self.b['traces']=[trace.copy()];self.b['traces'][0][3]=0
        self.assertEqual(self.compare(),'PASS')
        self.a['traces'][0][6]^=1;self.b['traces'][0][6]^=1
        self.assertEqual(self.compare(),'INVALID_CAPTURE')

    def test_rng_reseed_and_thread_order(self):
        self.a['traces']=[[1,0,0,1,1,100,100,0]]
        self.b['traces']=[[1,0,0,0,99,100,100,0]]
        self.assertEqual(self.compare(),'PASS')
        self.b['traces'][0][7]=1
        self.assertEqual(self.compare(),'DIFF_RNG')

    def test_adapter_overflow_and_unsupported_fail_closed(self):
        for code,status in [(5,'INVALID_CAPTURE'),(38,'UNSUPPORTED'),(100,'INVALID_CAPTURE')]:
            self.a['error']=code;self.assertEqual(self.compare(),status)

    def test_format_and_truncation(self):
        self.a['masks']=[0x7c00,0x3e0,31]
        self.assertEqual(self.compare(),'UNSUPPORTED')
        self.a['pixels']=b'';self.assertEqual(self.compare(),'INVALID_CAPTURE')

    def test_scene_and_schedule_differences(self):
        self.a['scene']=1;self.assertEqual(self.compare(),'DIFF_STATE')
        self.b['epoch']=1
        self.assertEqual(ls.compare_frame(self.a,self.b,None)['status'],'DIFF_SCHEDULE')

    def test_scene_transition_allows_different_arrival_times(self):
        comparator=ls.SceneTransitionComparator(100)
        self.assertEqual(comparator.compare(self.a,self.b,None,0,0)['status'],'PASS')
        state=(100000*1103515245+12345)&0xffffffff
        rng=lambda epoch,ms:[2,epoch,ms,0,100000,state,(state>>16)&32767,0]
        self.a.update(epoch=1,ms=20,scene=1);self.b.update(epoch=1,ms=20)
        self.a['traces']=[rng(1,20)]
        pending=comparator.compare(self.a,self.b,None,1,20)
        self.assertEqual((pending['status'],pending['scene_transition']),('PASS','pending'))
        self.a.update(epoch=2,ms=40);self.b.update(epoch=2,ms=40,scene=1)
        self.a['traces']=[];self.b['traces']=[]
        pending=comparator.compare(self.a,self.b,None,2,40)
        self.assertEqual((pending['status'],pending['scene_transition']),('PASS','pending'))
        self.a.update(epoch=3,ms=60);self.b.update(epoch=3,ms=60)
        self.b['traces']=[rng(3,60)]
        aligned=comparator.compare(self.a,self.b,None,3,60)
        self.assertEqual((aligned['status'],aligned['scene_transition']),('PASS','aligned'))
        self.assertEqual(comparator.completed[0]['paths'],[[0,1],[0,1]])
        self.assertEqual([side[-1]['ms'] for side in comparator.completed[0]['arrivals']],[20,40])

    def test_scene_transition_rejects_different_path_rng_and_incomplete_end(self):
        comparator=ls.SceneTransitionComparator(100)
        comparator.compare(self.a,self.b,None,0,0)
        self.a.update(epoch=1,ms=20,scene=1);self.b.update(epoch=1,ms=20)
        self.assertEqual(comparator.compare(self.a,self.b,None,1,20)['status'],'PASS')
        self.a.update(epoch=2,ms=40);self.b.update(epoch=2,ms=40,scene=2)
        result=comparator.compare(self.a,self.b,None,2,40)
        self.assertEqual((result['status'],result['reason']),('DIFF_STATE','scene_transition_path'))

        comparator=ls.SceneTransitionComparator(100)
        self.a,self.b=frame(),frame(2);comparator.compare(self.a,self.b,None,0,0)
        self.a.update(epoch=1,ms=20,scene=1);self.b.update(epoch=1,ms=20)
        result=comparator.compare(self.a,self.b,None,1,20,terminal=True)
        self.assertEqual((result['status'],result['reason']),('DIFF_STATE','scene_transition_incomplete'))

    def test_script_rejects_ambiguous_order_unknown_keys_and_coordinates(self):
        for events in [[dict(ms=0,kind=1,x=1,y=1)], [dict(ms=10,kind=1,x=640,y=0)],
                       [dict(ms=10,kind=4,key=999)], [dict(ms=10,kind=4,key=13)]*2]:
            with self.assertRaises(ValueError):ls.validate_events(events)
        self.assertEqual(ls.validate_events([dict(ms=1,kind=4,key=13)])[0]['id'],1)

    def test_shared_header_and_submission_bounds(self):
        endpoint=object.__new__(ls.Endpoint);endpoint.memory=bytearray(ls.SIZE)
        with self.assertRaises(ValueError):endpoint.snapshot()
        h=[0]*64;h[0]=ls.MAGIC;h[1]=1;h[39]=4097;endpoint.memory[:256]=ls.HEADER.pack(*h)
        with self.assertRaises(ValueError):endpoint.snapshot()
        h[39]=0;h[46]=1;endpoint.memory[:256]=ls.HEADER.pack(*h)
        with self.assertRaises(ValueError):endpoint.snapshot()
        struct.pack_into('<8I',endpoint.memory,ls.FRAME_OFFSET,0,0,9,ls.PIXEL_BYTES,0xf800,0x7e0,31,0)
        self.assertEqual(len(endpoint.snapshot()['submissions']),1)

    def test_controller_watchdog_and_crash(self):
        endpoint=object.__new__(ls.Endpoint);endpoint.k=Mock();endpoint.k.WaitForSingleObject.return_value=258
        endpoint.ready=1;endpoint.role='test'
        with self.assertRaises(TimeoutError):endpoint.receive(timeout=0)
        process=Mock();process.poll.return_value=7
        with self.assertRaises(ChildProcessError):endpoint.receive(process)

    def test_repeat_keeps_transient_submission_changes(self):
        r=dict(frames=[dict(original=self.a)])
        before=copy.deepcopy(repeat.signature(r,'original'))
        self.a['submissions']=[dict(sha256='transient-change')]
        self.assertNotEqual(before,repeat.signature(r,'original'))

    def test_unsampled_frames_keep_hashes_without_raw_files(self):
        self.a['submissions']=[dict(epoch=0,ms=0,source=1,masks=self.a['masks'],
            seq=0,sha256='submission',pixels=bytes(ls.PIXEL_BYTES))]
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            saved=ls.save_frame(folder,0,self.a,persist_raw=False)
            self.assertEqual(list(folder.iterdir()),[])
            self.assertNotIn('file',saved)
            self.assertEqual(saved['submissions'][0]['sha256'],'submission')
            self.assertNotIn('file',saved['submissions'][0])
            self.assertEqual(repeat.stored_files(saved),[])

    def test_cleanup_removes_only_staged_resource_copies(self):
        with tempfile.TemporaryDirectory() as temporary:
            out=Path(temporary)
            for path in ls.staging_paths(out):
                path.mkdir(parents=True);(path/'resource.mkf').write_bytes(b'data')
            evidence=out/'original'/'events.csv';evidence.write_text('kept')
            ls.cleanup_staging(out)
            self.assertTrue(evidence.is_file())
            self.assertTrue(all(not path.exists() for path in ls.staging_paths(out)))

    def test_failed_stage_removes_the_new_output_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);source=root/'source';dx=root/'dx';out=root/'out'
            source.mkdir();dx.mkdir()
            (source/'Rich4.exe').write_bytes(b'exe');(dx/'dxwnd.dll').write_bytes(b'dll')
            (source/'V3.11').mkdir();(source/'V3.11/Over.avi').write_bytes(b'over')
            (root/'bin').mkdir();(root/'bin/RICH4.CFG').write_bytes(bytes(4))
            args=Mock(data_dir=source,dxwnd_dir=dx,keep_staging=False)
            def digest(path):
                return ls.EXE_SHA if Path(path).name=='Rich4.exe' else ls.DXWND_SHA
            with patch.object(ls,'ROOT',root),patch.object(ls,'sha',side_effect=digest),\
                    patch.object(ls.subprocess,'check_output',return_value=''),\
                    patch.object(ls.shutil,'copy2',side_effect=OSError('copy failed')):
                with self.assertRaises(OSError):ls.stage(args,out)
            self.assertFalse(out.exists())

    def test_original_hook_audit_rejects_lost_log_and_removed_hook(self):
        rows=['1,0,0','1,1,0']+['24,0,0']*15+['21,0,0']
        def audit(data):
            path=Mock();path.open.return_value=io.StringIO('type,result,a\n'+'\n'.join(data))
            return ls.audit_original_log(path)
        self.assertIsNone(audit(rows))
        self.assertIsNotNone(audit(rows[:-1]))
        self.assertIsNotNone(audit(rows[:-1]+['21,0,1']))
        self.assertIsNotNone(audit(rows[:2]+rows[3:]))

if __name__=='__main__':unittest.main()
