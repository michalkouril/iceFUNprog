## Recompile

1. open in MPLAB IDE
2. click on MCC icon
3. click on Project Resources -> Generate
4. click on build

## Programming PIC16LF1459

Using a PIC programmer (Pickit, etc.) and MPLAB IDE/IPE

Pickit 3 wiring:
1. MCLR/Vpp     -> RST (RA3)   -> iceFUN (29)
2. Vdd target   -> 3v3         -> iceFUN (28)
3. Vss ground   -> GND         -> iceFUN (27)
4. ICSPDAT      -> AD1 (RA0)   -> iceFUN (26)
5. ICSPCLK      -> AD2 (RA1)   -> iceFUN (25)
6. PGM (LVP)

