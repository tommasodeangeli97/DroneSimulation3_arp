# DroneSimulation3_arp
The third Advanced and Robotics Programming assignament:  is a drone simulator implemented totally in c language and controllable in forces, the window is created using the *ncurses* library, in the environment random targets and obstacles spawn communicating to the server through sockets, the obstacles create a repulsive force, the *SCORE* is a funtion of how many targets and obstacles are encountered by the drone. The drone is controlled by key inputs that change the total force applied to the drone.

## Description
The program is composed by eight different processes that cooperate in real time and share informations.

The communication between the `server` and `target` and between the `server` and the `obstacles` is implemented using socket; the communication between the `server`, the `drone` and the `keyboard` is implemented using pipes and shared memory.


The final result gives to the user the possibility to move a drone in a free environment where the friction force, the forces intruduced to control it and the obstacles' repulsive forces are acting.
Furthermore the drone is unable to go out the screen.

These are the key to control the robot, however the `master` process visualises them befor starting the application:
```
UP 'e'
UP_LEFT 'w'
UP_RIGHT 'r'
RIGHT 'f'
0 FORCES 'd'
LEFT 's'
DOWN 'c'
DOWN_LEFT 'x'
DOWN_RIGHT 'v'
QUIT 'q'
```
