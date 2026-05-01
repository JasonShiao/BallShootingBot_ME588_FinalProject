## Description
Once the game is started, the ball launcher bot will depart from home area and follow a line corridor 
and when a horizontal line is detected on either side, it detects the IR beacon frequency and launch 
a ball to hill box if the beacon doesn't match the team the robot belongs to. If the hill changes loyalty,
the robot will continue to move until the next junction. Repeat the process until timeout. If the ball is
not loaded when trying to launch, it will go back home to reload the bucket.

* Inputs:
  * A team toggle button to select whether the robot belongs to team RED or team BLUE.
  * A game start button.

* Display:
  * A LED as game active indicator
  * A LED as ball launching indicator
  * A on-board RGB LED as team indicator

* Sensing:
  * QTR-8RC for line following
  * QTR-1A for junction (horizontal line) detection

## FSM
This project utilizes hierarchical FSM to make the code manageable and scalable.
The hierarchy of FSM is as follows:
```
Startup State

Inactive State
│
├── Idle State
├── ManualControl State
└── Error State

GameActive State
│
├── Navigation State
│   ├── MoveToNextJunction State
│   └── BackHome State
│
├── HillInteraction State
│   ├── CheckHillLoyalty State
│   ├── BallLoading State
│   └── BallLaunching State
│
└── WaitBucketReload State
```
States are grouped by similar actions/properties and the handle(), onExit(), onEnter() member functions of group states
are filled with common actions for each child state which can pretty much reduce duplicate code.


## Other Hardware
* ESP32-S3 x1
* 12V to 5V step down buck converter x1
* Motor driver x1 (recommend a modern one instead of L298N)
* 12V DC motor x2
* 5V Servo x2 for ball loader and ball launcher
* TEKT-5400S IR phototransistor x1
* opamp x2
* caster wheel x2
* wheel x2

