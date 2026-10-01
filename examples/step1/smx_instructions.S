    .text

    .globl smx_contx_demo
    .type smx_contx_demo, @function
smx_contx_demo:
    # a0 is both the input context and the output context.
    smx.contx a0, a1, a2
    ret
    .size smx_contx_demo, .-smx_contx_demo

    .globl smx_acc_demo
    .type smx_acc_demo, @function
smx_acc_demo:
    # a0 is both the input accumulator and the output accumulator.
    smx.acc a0, a1, a2
    ret
    .size smx_acc_demo, .-smx_acc_demo

    .globl smx_exp_demo
    .type smx_exp_demo, @function
smx_exp_demo:
    smx.exp a0, a1, a2
    ret
    .size smx_exp_demo, .-smx_exp_demo

    .globl smx_sigm_demo
    .type smx_sigm_demo, @function
smx_sigm_demo:
    smx.sigm a0, a1, a2
    ret
    .size smx_sigm_demo, .-smx_sigm_demo
