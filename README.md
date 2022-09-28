Here is a test task that an interviewee needs to accomplish as a part of a job
interview. This task is from a company which produces telecom equipment.
I've found its interesting that is why I publish the task and my solution.

## Test task

There is a network of sensors, sensors measure a temperature and a brightness.
The sensors are polled for these parameters by a controller. The controller
gathers parameters, calculates averaged values and sends averaged temperature
and arbitrary text to sensors. Each sensor shows received info on its output
device.

The network is not reliable and sensors can be unreachable by the controller.
Lost sensors are left in their local network where they need to choose a master
device which takes the role of the controller. The chosen master acts like a
controller, it polls other sensors, gathers parameters, sets averaged info and
text to accessible sensors. The master failure can be repeated and the network
of sensors should be self orginized again.

When the connection with the controller or with the previous master is restored
the current master becomes a sensor.

Implement an emulator for a controller and sensors.


## Solution

Suppose that there is no controller and sensors need to choose a master device.
There should be a major attribute that helps to detect who becomes a master.
The network address is such attribute.

A sensor can be in several states: UNKNOWN, MASTER, SLAVE.

* UNKNOWN - needs to detect the role (MASTER or SLAVE)

* SLAVE - polled by MASTER

* MASTER - poll other sensors


Use 2 network packet types:

* HELLO - discovery packet, detects other host reachability

* GET - gets the current temperature and brightness, may content an averaged
brigtness and a text message to show on the sensors output device (so get may
also be set command).


At a start time or at a time when the lost of a controller is detected each
sensor comes to UNKNOWN state. In this state the current device is interested
in sensors with addresses that are HIGHER than its own address. The device
connects to sensors with HIGHER address and when the connection is established
it sends HELLO message. In response HELLO is expected. If at least one sensor
responses with HELLO the current sensor drops all HELLO connections (the higher
address sensor is found, since it gets a single response other responses are
not important) and changes its state to SLAVE. If there are no valid responses
it means that the sensor has the highest address and it changes its state to
MASTER. In UNKNOWN state while polling other sensors the device keeps accepting
connections and receiving HELLO or GET from other sensors. HELLO is
echoed, but if the device receives GET it means that other master or the
controller is back and the device needs to drop all HELLO connections and
changes its state to SLAVE.

In SLAVE state the sensor only accepts connections from other sensors and
replies to HELLO or GET. It also starts timer to detect when there is no
messages for long time. When the timeout is triggered the sensor changes it
state to UNKNOWN and detects its new role (read about UNKNOWN state).

In MASTER state the sensor polls addresses that are LESS than its own address.
The sensor accepts connections from other sensors as before. If it receives GET
it means that other master or the controller is back and the sensor changes
its state to SLAVE.

What about a controller? The controller is just the same program. It is a
device which doesn't accept connections from other sensors and can't change its
state, it is alway in controller/master state. It polls sensors and insists that
it is a master device for other. The controller can have arbitrary address. The
network can have several controllers.


## Run

```
$ cd src
$ make
```

Start 5 progs with random address, the network will be self organized (terminal
produces too many logs use watch to detect progs states):

```
$ ./run.sh
```


Run a controller (address X must be different from what run.sh generated):

```
$ HOST_ADDR=X CONTROLLER= ./prog
```

Run watch to check prog states:

```
$ watch -n 1 "ps -eo pid,cmd | grep '[p]rog.*\(SLAVE\|MASTER\|CONTR\)'"
```

The state of each program is outputted to cmd. This is sensors output device
where the state, an averaged temperature and a message are printed.

To test state transition kill prog, check reported states in the watch terminal.
Then run prog with a higher address or in a controller mode the network should
be self organized.


