.section .text
.align  2

.global arch_interrupt_handler_asm
.type   arch_interrupt_handler_asm, @function

.equ CPU_ID,           0
.equ CPU_KERNEL_STACK, 8

.equ TF_ZERO,          0
.equ TF_RA,            8
.equ TF_USER_SP,      16
.equ TF_GP,            24
.equ TF_TP,            32

.equ TF_T0,            40
.equ TF_T1,            48
.equ TF_T2,            56

.equ TF_S0,            64
.equ TF_S1,            72

.equ TF_A0,            80
.equ TF_A1,            88
.equ TF_A2,            96
.equ TF_A3,           104
.equ TF_A4,           112
.equ TF_A5,           120
.equ TF_A6,           128
.equ TF_A7,           136

.equ TF_S2,           144
.equ TF_S3,           152
.equ TF_S4,           160
.equ TF_S5,           168
.equ TF_S6,           176
.equ TF_S7,           184
.equ TF_S8,           192
.equ TF_S9,           200
.equ TF_S10,          208
.equ TF_S11,          216

.equ TF_T3,           224
.equ TF_T4,           232
.equ TF_T5,           240
.equ TF_T6,           248

.equ TF_SCAUSE,       256
.equ TF_STVAL,        264
.equ TF_SEPC,         272
.equ TF_SSTATUS,      280

.equ TF_SIZE,         288


arch_interrupt_handler_asm:
    mv      t0, sp

    csrr    t1, sstatus
    andi    t1, t1, 0x100
    bnez    t1, .from_kernel

.from_user:
    csrrw   tp, sscratch, tp
    ld      sp, CPU_KERNEL_STACK(tp)
    j       .trap_stack_ready

.from_kernel:
    mv      sp, t0


.trap_stack_ready:
    addi    sp, sp, -TF_SIZE

    sd      zero, TF_ZERO(sp)
    sd      ra,   TF_RA(sp)

    sd      t0,   TF_USER_SP(sp)

    sd      gp,   TF_GP(sp)

    csrr    t0, sscratch
    bnez    t0, .save_user_tp

    sd      tp, TF_TP(sp)
    j       .tp_saved

.save_user_tp:
    sd      t0, TF_TP(sp)

.tp_saved:
    sd      t0, TF_T0(sp)
    sd      t1, TF_T1(sp)
    sd      t2, TF_T2(sp)

    sd      s0, TF_S0(sp)
    sd      s1, TF_S1(sp)

    sd      a0, TF_A0(sp)
    sd      a1, TF_A1(sp)
    sd      a2, TF_A2(sp)
    sd      a3, TF_A3(sp)
    sd      a4, TF_A4(sp)
    sd      a5, TF_A5(sp)
    sd      a6, TF_A6(sp)
    sd      a7, TF_A7(sp)

    sd      s2,  TF_S2(sp)
    sd      s3,  TF_S3(sp)
    sd      s4,  TF_S4(sp)
    sd      s5,  TF_S5(sp)
    sd      s6,  TF_S6(sp)
    sd      s7,  TF_S7(sp)
    sd      s8,  TF_S8(sp)
    sd      s9,  TF_S9(sp)
    sd      s10, TF_S10(sp)
    sd      s11, TF_S11(sp)

    sd      t3, TF_T3(sp)
    sd      t4, TF_T4(sp)
    sd      t5, TF_T5(sp)
    sd      t6, TF_T6(sp)

csrr    t0, scause
    sd      t0, TF_SCAUSE(sp)

    csrr    t0, stval
    sd      t0, TF_STVAL(sp)

    csrr    t0, sepc
    sd      t0, TF_SEPC(sp)

    csrr    t0, sstatus
    sd      t0, TF_SSTATUS(sp)

    mv      a0, sp
    call    arch_interrupt_handler

    mv      sp, a0

    ld      t0, TF_SEPC(sp)
    csrw    sepc, t0

    ld      t0, TF_SSTATUS(sp)
    csrw    sstatus, t0

    ld      ra,   TF_RA(sp)
    ld      gp,   TF_GP(sp)

    ld      t0,   TF_T0(sp)
    ld      t1,   TF_T1(sp)
    ld      t2,   TF_T2(sp)

    ld      s0,   TF_S0(sp)
    ld      s1,   TF_S1(sp)

    ld      a0,   TF_A0(sp)
    ld      a1,   TF_A1(sp)
    ld      a2,   TF_A2(sp)
    ld      a3,   TF_A3(sp)
    ld      a4,   TF_A4(sp)
    ld      a5,   TF_A5(sp)
    ld      a6,   TF_A6(sp)
    ld      a7,   TF_A7(sp)

    ld      s2,   TF_S2(sp)
    ld      s3,   TF_S3(sp)
    ld      s4,   TF_S4(sp)
    ld      s5,   TF_S5(sp)
    ld      s6,   TF_S6(sp)
    ld      s7,   TF_S7(sp)
    ld      s8,   TF_S8(sp)
    ld      s9,   TF_S9(sp)
    ld      s10,  TF_S10(sp)
    ld      s11,  TF_S11(sp)

    ld      t3,   TF_T3(sp)
    ld      t4,   TF_T4(sp)
    ld      t5,   TF_T5(sp)
    ld      t6,   TF_T6(sp)

    ld      t0, TF_SSTATUS(sp)
    andi    t0, t0, 0x100
    bnez    t0, .return_to_kernel


.return_to_user:
    csrrw   tp, sscratch, tp

    j       .restore_sp


.return_to_kernel:
    # nothing to do
    # i might actually consider removing this label

.restore_sp:
    ld      sp, TF_USER_SP(sp)

    sret


.size arch_interrupt_handler_asm, .-arch_interrupt_handler_asm
