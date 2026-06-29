/* x86 only; original entry bytes and stack arguments verified in IDA.
   EFLAGS and all registers at the original function's return are retained. */
.text
.macro STUB name, kind, target, nargs
.globl _\name
_\name:
 pushfl
 pushal
 pushl 40(%esp)
 pushl 40(%esp)
 pushl $\kind
 call _internal_enter
 addl $12,%esp
 popal
 popfl
.if \nargs
 pushl 4(%esp)
.endif
 call *_\target
.if \nargs
 leal 4(%esp),%esp
.endif
 pushfl
 pushal
 pushl 28(%esp)
 call _internal_leave
 addl $4,%esp
 popal
 popfl
 ret
.endm
STUB stub_rand,0,tramp_rand,0
STUB stub_seed,1,tramp_seed,1
STUB stub_wait,2,tramp_wait,1
STUB stub_wait_input,3,tramp_wait_input,1
STUB stub_audio,4,tramp_audio,0
