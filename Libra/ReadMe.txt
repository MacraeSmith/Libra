Prototype Libra Epilogue - Made by Macrae Smith Cohort 34

Instructions:
---------------------------------------------
Open up the Libra_Release_x64.exe file in the Run folder to launch the game

or

Open the Libra Solution in Visual Studio 2022 and press ctrl + shift + B to build, and then F5 to play the game. 

(If you are building from VS 2022 for the first time, you will need to right click on the Starship Solution in Editor and click Properties->ConfigurationProperties->Debugging and change the Command field to $(TargetFileName) and the Working Directory filed to $(SolutionDir)Run/)

Keyboard Controls:
---------------------------------------------
WASD - Move
IJKL - Rotate Turret
SPACEBAR - Fire bullet
R - Toggle weapons (Gun/FlameThrower)
P - Enter game from attract screen/ pause and unpause when in gameplay
ESC - Go back/Quit Game
N - respawn when dead

T - slow down time to 1/10th
Y- speed up time
O - Run one frame and then pause
F1 - Turn on/off debug drawings
F2 - Invincible mode
F3 - No clip mode - turn off wall collisions
F4 - Show entire map in camera view
F6 - Rotate through debug distance maps
F7 - Show tile debug info
F8 - Restart Game
F9 - Go to next map
~ - Toggle opening/closing dev console
1 - Switch between full screen and quarter screen dev console
2 - Print test error message in dev console
3 - Print test warning message in dev console
4 - Print test event to dev console
5 - Execute last line on dev console (only does something if the last line is an event)

Xbox Controller Controls:
---------------------------------------------
Right Stick - Move
Left Stick - Rotate turret
START - Enter game from attract screen/ pause and unpause when in gameplay/ respawn when dead
Right trigger - Fire bullet
Right bumper button - Toggle weapon
B Button - Go back/Quit Game

Deep Learning:
---------------------------------------------
The entire Libra project really grew me as a developer. From managing tile grids, procedural maps, raycasts, XML data, and over all better organization of entities. What stuck out to me in assignment 8 was the event system and dev console. Taking my understanding of reading string data to the next level, it was challenging but rewarding to create systems that can read lines of text and execute event functions based upon them. Furthermore creating the event system opened up ideas of cool things that it can be utilized for even though there were not too many uses for it yet. The examples I have currently in Libra are: Exiting the windows application, healing the player through dev console, regenerating pathfinding distance field maps for every enemy whenever a Scorpio dies, and changing which Leo is being tracked by the debug screen whenever the current one dies. Not all of these necessarily required the use of the event system, but I wanted to test out different utilizations of it.

Another challenge I faced in this assignment had to do with improved NPC pathfinding. This required me to rewrite some big portions of their current behavior, and led to some weird edge case bugs when switching between following a set path and chasing the player directly if they were close enough. This helped me make sure I was intentional about what each line of code was doing in my Enemy logic and forced a cleaner, more organized logic flow.

I had a lot of fun creating the Aquarius and Gemini entity types, but in wanting to make their behavior very different from pre-existing entities, this added to some of the bugs mentioned earlier. Creating a system for Aquarius that created "overrides" for tiles so that they would turn into water for a short time before reverting back, opened up challenges for pathfinding and player mobility around the map as well as the need for a system to keep track of the age of the override and the tile's old data. This made me appreciate the organized system for tiles I had created in previous assignments. The Gemini "twins" required references to each other which also provided unique challenges of changing behavior to respond to what the other twin did. Knowing what I know now, I may have set up the architecture of entities slightly differently to prepare for these unique challenges, but also tackling how much adding one entity affected the rest of them was something that forced me to grow as a developer.


Known Bugs:
---------------------------------------------
On maps that have the Aquarius enemy type, having other enemies that cannot traverse water creates weird behavior with their pathfinding. Because the Aquarius is consistently changing the map by adding water tiles, this blocks their path. I have purposefully not included any maps where Aquarius exist with Leos or Aries do avoid this issue.

When in debug fast mode, there are rare instances where the player accidently gets stuck in a wall.

When in debug fast mode, there are instances where the audio creates weird issues because too many sound effects are playing at once.

