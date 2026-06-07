
    PRESERVE8
    THUMB
    
    
    AREA    |.text|, CODE, READONLY, ALIGN=2

    EXPORT  Read_ADC_Assembly

Read_ADC_Assembly
    ; Load ADC0 SS3 FIFO Address
    LDR     R1, =0x400380A8   

    ; Load current read into R0
    LDR     R0, [R1]          

    ;Get the 12-bit read
    MOVW    R2, #0x0FFF       
    AND     R0, R0, R2        

    ; 4-bit shift
    LSL     R0, R0, #4        

    ;Return, R0 is returned as default return register.
    BX      LR                

    ALIGN
    END