# Note

These data wrappers cover the underlying implementation, so in the future
the implementations can be polished further.

* session: session storage
* pub_sub: subscriptions, topic pattern matching, worker queue ack mode and credit
* pending_acks: worker queue messages delivered but not acknowledged yet (owned until ACK, NACK, time-out or subscription end)