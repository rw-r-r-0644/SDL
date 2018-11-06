; $MODE = "UniformRegister"
; $UNIFORM_VARS[0].name = "u_viewSize"
; $UNIFORM_VARS[0].type = "float2"
; $UNIFORM_VARS[0].count = 1
; $UNIFORM_VARS[0].offset = 4
; $UNIFORM_VARS[0].block = -1
; $UNIFORM_VARS[1].name = "u_texSize"
; $UNIFORM_VARS[1].type = "float2"
; $UNIFORM_VARS[1].count = 1
; $UNIFORM_VARS[1].offset = 0
; $UNIFORM_VARS[1].block = -1
; $ATTRIB_VARS[0].name = "a_position"
; $ATTRIB_VARS[0].type = "float2"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "a_texCoordIn"
; $ATTRIB_VARS[1].type = "float2"
; $ATTRIB_VARS[1].location = 1
; $NUM_SPI_VS_OUT_ID = 1
; $SPI_VS_OUT_ID[0].semantic_0 = 0

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(16)
      0  x: ADD         R127.x, -R2.y,  C0.y
         z: MOV         R0.z,  0.0f
         w: ADD         R127.w, -R1.y,  C1.y      VEC_120
         t: RCP_e       ____,  C1.x
      1  y: MUL_e*2     ____,  R1.x,  PS0
         w: MOV         R0.w,  (0x3F800000, 1.0f).x
         t: RCP_e       ____,  C1.y
      2  x: MUL_e*2     ____,  R127.w,  PS1
         t: ADD         R0.x,  PV1.y, -1.0f
      3  y: ADD         R0.y,  PV2.x, -1.0f
         t: RCP_e       ____,  C0.x
      4  x: MUL_e       ____,  R2.x,  PS3
         t: RCP_e       ____,  C0.y
      5  x: MOV         R2.x,  PV4.x
         z: MUL_e       ____,  R127.x,  PS4
      6  y: MOV         R2.y,  PV5.z
02 EXP_DONE: POS0, R0
03 EXP_DONE: PARAM0, R2.xyzz  NO_BARRIER
04 ALU: ADDR(48) CNT(1)
      7  x: NOP         ____
05 NOP NO_BARRIER
END_OF_PROGRAM
