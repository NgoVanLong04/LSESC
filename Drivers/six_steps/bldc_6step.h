#ifndef BLDC_6STEP_H_
#define BLDC_6STEP_H_

/* Commutation offset 0..5. Two offsets 3 apart spin in opposite directions.
 * Tune it live (debugger watch) at low duty, see notes. */
extern volatile int g_comm_offset;

/* 0 or 1. If no offset makes the motor spin, set this to 1 and sweep the
 * offsets again (same as swapping two motor phase wires or two hall wires). */
extern volatile int g_comm_reverse;

/* Spin direction: +1 uses g_comm_offset, -1 uses g_comm_offset_ccw. */
extern volatile int g_comm_dir;

/* Best offset for the counter-clockwise direction. It is NOT necessarily
 * g_comm_offset + 3, see the notes about lead angle. */
extern volatile int g_comm_offset_ccw;

/* Call once after MX_TIM1_Init() and drv_start(). All six outputs stay off (Hi-Z). */
int bldc_init(void);

/* All three phases Hi-Z (INH = INL = 0). */
void bldc_all_float(void);

/* phase: 1..6 from read_current_phase(), duty: 0..1.
 * Invalid phase (e.g. -1) puts all phases in Hi-Z. */
int set_phase(int phase, float duty);


#endif /* BLDC_6STEP_H_ */
