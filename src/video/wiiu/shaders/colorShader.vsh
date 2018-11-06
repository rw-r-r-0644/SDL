; $MODE = "UniformRegister"
; $UNIFORM_VARS[0].name = "u_viewSize"
; $UNIFORM_VARS[0].type = "float2"
; $UNIFORM_VARS[0].count = 1
; $UNIFORM_VARS[0].offset = 0
; $UNIFORM_VARS[0].block = -1
; $ATTRIB_VARS[0].name = "a_position"
; $ATTRIB_VARS[0].type = "float2"
; $ATTRIB_VARS[0].location = 0

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(9)
      0  x: ADD         R0.x, -R1.y,  C0.y
         z: MOV         R2.z,  0.0f
         w: MOV         R2.w,  (0x3F800000, 1.0f).x
         t: RCP_e       ____,  C0.x
      1  z: MUL_e*2     ____,  R1.x,  PS0
         t: RCP_e       ____,  C0.y
      2  x: MUL_e*2     ____,  R0.x,  PS1
         t: ADD         R2.x,  PV1.z, -1.0f
      3  y: ADD         R2.y,  PV2.x, -1.0f
02 EXP_DONE: POS0, R2
03 EXP_DONE: PARAM0, R0.____
04 ALU: ADDR(41) CNT(1)
      4  x: NOP         ____
05 NOP NO_BARRIER
END_OF_PROGRAM
