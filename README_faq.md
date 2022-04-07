# FAQ

1. Sometimes bigger latency is observed on the first run (especially after reboot, or from 'cold' system').
 It is suggested that the first latency data after reboot shd ignored. It seems the cache/data pipe needs to
 be warmed for best performance. Due to this environment factor, user could for example have 5 subsequent runs,
 and just take the results from the last 3 or 4 values.

2. A min of 3-5 sec (not exceeding 10 sec) between running run B and run A.

3. It is suggested to reboot a system before running tests. However, between run of the same type of test,
rebooting is optional.

For example:
* running 3 times opcua-pkt2x :
  1. 1st run : reboot, init, setup, run
  2. 2nd run : setup (optional), run
  3. 3rd run : setup (optional), run

Furthermore, switching between PKT2 and PKT 3, we have observed that it is ok not to reboot (same AF_PACKET data transmission).
Same goes for switching between XDP2 and XDP3,  it  it is ok not to reboot ( same AF_XDP data transmission).

Switching between running pkt and XDP, fresh reboot would be best.

4. Opcua pubsub error seen occasionally :

xdp socket:
[2021-06-23 00:45:33.281 (UTC+0000)] error/server       PubSub connection send failed. xsk_prod_reserve failed.
[2021-06-23 00:45:33.281 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage
[2021-06-23 00:45:33.281 (UTC+0000)] error/server       PubSub connection send failed. xsk_prod_reserve failed.
[2021-06-23 00:45:33.281 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage
[2021-06-23 00:45:33.282 (UTC+0000)] error/server       PubSub connection send failed. xsk_prod_reserve failed.
[2021-06-23 00:45:33.282 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage
[2021-06-23 00:45:33.282 (UTC+0000)] error/server       PubSub connection send failed. xsk_prod_reserve failed.
[2021-06-23 00:45:33.282 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage

af-packet:
[2021-10-25 16:49:18.448 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage
[2021-10-25 16:49:18.448 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage
[2021-10-25 16:49:18.449 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage
[2021-10-25 16:49:18.449 (UTC+0000)] warn/server        PubSub Publish: Could not send a NetworkMessage

If this happen, rerun : init , setup , and run for both boards (or even reboot if you have the luxury to do so).
Please ensure you have a gap between running B and A. The opcua server on both sides (A and B) have to be ready
before you send packets to it.

Changing this json parameter would have an effect as well: "publish_delay_sec"
This is a delay for which the pubsub server will wait before starting to publish.

Please ensure the system is in sync as well, because these packets are sent with timestamp for transmission.

5. It is good to have  tail -f /var/log/ptp4l.log -f /var/log/ptp4l2.log in another terminal to ensure the ptp
is in sync. If it is not, it is normal that you have big latency!

6. Suggestion of watching the interrupst : watch -d -n 1 'cat /proc/interrupts | grep eth'
(to see the interrupts firing away) !!
Note for XDP 5.10 and above, you will not see any rx interrupts on XDP queues due to napi deferral in place.
