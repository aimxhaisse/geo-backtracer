# Integration with mobile data

One requirement for this approach to be effective is to have aligned
points as input: i.e: have mobile applications emit GPS points during
a narrow window of time (for instance, at second 30 of every minute).
This makes it possible to handle complex situations (i.e: a car
following a bus for instance) without having to take into account
directions and speed of users.

Typically, in pseudo-code, here is how a mobile app could implement
such an approach:

    kSendRateSeconds = 60

    while (true) {
	  # Get position at second 0 of the minute, all phones are
	  # calibrated on time with GPS data, so they have a somewhat
	  # consistent view of time. This means they can all get a point at
	  # second 0, in a synchronized way, with little effort.
	  #
	  # This makes correlation easy: we can assume that two points
	  # that are close were close at the same time and then movement
	  # doesn't matter: we don't need to compute speed or direction.
	  current_time_ms = now()
	  padded_time_ms = (current_time_ms / (1000 * kSendRateSeconds)) * (1000 * kSendRateSeconds)
	  sleep_ms(current_time_ms - padded_time_ms)

	  # Send GPS position.
	  current_position = get_position()
	  send_position(current_position)

	  # Move a bit the clock, so that we pick the next cycle.
	  sleep_ms((kSendRateSeconds / 2) * 1000)
    }

A phone that is not moving should not be sending points, but could be
sending instead: here was my position for the last 10 hours ; this can
be done by setting a duration in the API. This significantly reduces
the size of the database.
