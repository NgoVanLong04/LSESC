#ifndef HALL_COMM_H_
#define HALL_COMM_H_

#include <stdint.h>

/* Hall commutation with a tunable delay after each hall edge.
 *
 * The hall sensors of this motor are about 30 electrical degrees away from the
 * ideal 6-step commutation points, so commutating right on the edge is about
 * 30 degrees early. This module waits a fraction of the previous sector time
 * before it applies the new step.
 *
 * Uses TIM2 as a free-running 1 MHz counter, set up in hall_comm_init().
 * Do NOT enable TIM2 in CubeMX. */

/* Delay as a fraction of one hall sector (60 el. deg): 0 = commutate on the
 * edge (old behaviour), 0.5 = 30 el. deg. Clamped to 0..0.9. Tune live. */
extern volatile float g_comm_delay_frac;		/* used when g_comm_dir = +1 */
extern volatile float g_comm_delay_frac_ccw;	/* used when g_comm_dir = -1 */

/* Debug watch values */
extern volatile uint32_t g_comm_sector_us;	/* duration of the last hall sector */
extern volatile uint32_t g_comm_delayed;	/* number of delayed commutations so far */

/* Call once, after bldc_init(). */
void hall_comm_init(void);

/* Call from the hall EXTI ISR. phase: 1..6 or -1, step: signed hall step
 * (+1 / -1 for a normal edge, anything else = no timing information). */
void hall_comm_on_edge(int phase, int step);

/* Call from the control task every few ms, inside a critical section.
 * Applies the duty and takes over commutation while the motor is stopped. */
void hall_comm_refresh(float duty);

#endif /* HALL_COMM_H_ */
