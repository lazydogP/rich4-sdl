"""Integrity checks for evidence: malformed/incomplete runs must not pass."""
import contextlib
import csv
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

import r4diff

FIELDS='us duration_us type thread caller object result a b c d'.split()

def event(kind, **fields):
    row=dict.fromkeys(FIELDS,0)
    row.update(type=kind,thread=1,caller='0',object='0')
    row.update(fields)
    return row

def frame(path, seq=1, width=640, height=480, x=0, y=0, pixel=0xf800, masks=(0xf800,0x7e0,0x1f)):
    h=[0x31463452,seq,100,0,width,height,16,width*2,*masks,123,12,x,y,1]
    path.write_bytes(struct.pack('<16I',*h)+struct.pack('<H',pixel)*(width*height))

def png_rows(data):
    offset=8;payload=b''
    while offset<len(data):
        size=struct.unpack('>I',data[offset:offset+4])[0]
        if data[offset+4:offset+8]==b'IDAT': payload+=data[offset+8:offset+8+size]
        offset+=12+size
    return zlib.decompress(payload)

class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.out=Path(self.temp.name)
        (self.out/'manifest.json').write_text(json.dumps(dict(capture='full',launcher_exit=0)))
        self.rows=[event(1),event(1,result=1)]+[event(24) for _ in range(8)]+[event(21)]

    def tearDown(self): self.temp.cleanup()

    def report(self):
        with (self.out/'events.csv').open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=FIELDS);w.writeheader();w.writerows(self.rows)
        with contextlib.redirect_stdout(io.StringIO()): return r4diff.report(self.out)

    def test_rgb565_truncation_and_patch_coordinates(self):
        frame(self.out/'frame-000001.r4f')
        frame(self.out/'frame-000002.r4f',seq=2,width=2,height=1,x=17,y=23,pixel=0x7e0)
        self.rows += [event(19,a=1),event(19,a=2)]
        result=self.report()
        self.assertEqual(result['status'],'OBSERVED')
        self.assertTrue(result['preview_complete'])
        raw=png_rows((self.out/'frame-000002.png').read_bytes())
        start=23*(640*3+1)+1+17*3
        self.assertEqual(raw[start-3:start+9],bytes([248,0,0,0,252,0,0,252,0,248,0,0]))

    def test_truncated_frame_invalidates_run(self):
        (self.out/'frame-000001.r4f').write_bytes(b'bad')
        self.rows.append(event(19,a=1))
        self.assertEqual(self.report()['status'],'INVALID_CAPTURE')

    def test_missing_frame_invalidates_run(self):
        self.rows.append(event(19,a=1))
        self.assertEqual(self.report()['status'],'INVALID_CAPTURE')

    def test_orphan_patch_and_outside_patch_rejected(self):
        frame(self.out/'frame-000001.r4f',width=2,height=1)
        self.rows.append(event(19,a=1))
        self.assertEqual(self.report()['status'],'INVALID_CAPTURE')
        frame(self.out/'frame-000001.r4f')
        frame(self.out/'frame-000002.r4f',seq=2,width=2,height=1,x=639)
        self.rows.append(event(19,a=2))
        self.assertEqual(self.report()['status'],'INVALID_CAPTURE')

    def test_missing_end_dropped_events_and_failed_unlock(self):
        for mutation in ('end','lost','unlock','hook','launcher'):
            original=list(self.rows)
            if mutation=='end': self.rows=self.rows[:-1]
            elif mutation=='lost': self.rows[-1]=event(21,a=1)
            elif mutation=='unlock': self.rows.append(event(12,result=-2005532242))
            elif mutation=='hook': self.rows=self.rows[1:]
            else: (self.out/'manifest.json').write_text('{"launcher_exit": 5}')
            with self.subTest(mutation=mutation): self.assertEqual(self.report()['status'],'INVALID_CAPTURE')
            self.rows=original

    def test_rng_return_state_chain_and_reseed(self):
        state=(1*1103515245+12345)&0xffffffff
        rows=[event(17,a=0,b=1,c=1),event(16,a=1,b=state,result=(state>>16)&32767)]
        self.assertEqual(r4diff.validate_rng(rows)['status'],'VALID')
        rows[-1]['result'] ^= 1
        self.assertEqual(r4diff.validate_rng(rows)['status'],'INVALID')
        self.assertEqual(r4diff.validate_rng([])['status'],'NOT_EXERCISED')

    def test_zero_microseconds_count_and_nearest_rank_percentile(self):
        self.rows += [event(12,caller='244c',us=10,duration_us=0),event(12,caller='244c',us=20,duration_us=10)]
        span=self.report()['spans'][0]
        self.assertEqual(span['count'],2)
        self.assertEqual(span['mean_ms'],.005)
        self.assertEqual(r4diff.distribution(list(range(1,21)))['p95_ms'],.019)

    def test_sample_preview_is_not_complete(self):
        (self.out/'manifest.json').write_text('{"capture": "sample"}')
        frame(self.out/'frame-000001.r4f');self.rows.append(event(19,a=1))
        self.assertFalse(self.report()['preview_complete'])

    def test_bad_masks_rejected(self):
        for masks in ((0xf800,0xf800,0x1f),(0xf800,0x700,0x100000),(0xf800,0x500,0x1f)):
            path=self.out/'bad.r4f';frame(path,width=1,height=1,masks=masks)
            with self.assertRaises(ValueError): r4diff.decode_frame(path)

    def test_isolated_profile_has_only_target_zero(self):
        path=self.out/'source.ini'
        source='[target]\npath0=original.exe\nflagl0=8\nrenderer0=3\nflag0=740302881\npath10=other.exe\nrenderer10=4\n'
        path.write_text(source)
        text=r4diff.profile_text(path,self.out/'game'/'Rich4.exe',{'renderer0':'5'})
        self.assertIn('renderer0=5',text)
        self.assertIn('flagl0=32776',text)
        self.assertIn('flag0=740302881',text)
        self.assertNotIn('other.exe',text)
        self.assertEqual(path.read_text(),source)
        with self.assertRaises(ValueError): r4diff.profile_text(path,self.out/'a.exe',{'path0':'evil.exe'})

if __name__=='__main__': unittest.main()
