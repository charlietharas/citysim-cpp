Simulates transit riders in the New York City subway based on annual station ridership data.

### Data Sources
https://github.com/CityOfNewYork/nyc-geo-metadata
- [Station points data](https://data.cityofnewyork.us/Transportation/Subway-Stations/arq3-7z49)
- [Line points data](https://data.cityofnewyork.us/Transportation/Subway-Lines/3qz8-muuu)

[Line order data](https://new.mta.info/maps/subway-line-maps)

[Station ridership data](https://new.mta.info/agency/new-york-city-transit/subway-bus-ridership-2021)

### Future improvements
Possible short term performance improvements:
- pathfinding
	- path contraction to POIs/line nodes--add nodes along the same line as node neighbors, or add major transfer points (this has previously reduced performance)
	- multithreading/hardware utilization (likely not necessary)
- citizen update loop
	- delay updates/checks on citizens that aren't expected to get to their destination for a while (some sort of priority queue/other data structure)
	- path contraction post-generation (this has previously created issues and failed to improve performance, but does reduce memory usage significantly)
	- explore options for vectorization/individual update optimization (likely not helpful)

Possibilities for extension:
- rush hour (!)
- custom and accurate train counts, speeds (from https://new.mta.info/schedules, The Weekender)
- tweaks to pathfinding algorithm for better realism
	- dynamically update weights for algorithm using train ETAs (incorporate headways)
		- use D*?
	- adjust algorithm hyperparameters (and add variance to individual agent heuristics) for realistic choice-making
- drawing "complex lines" instead of straight lines between stations (I already have the data)
	- making trains follow those lines
	- giving trains basic physics (accel, decel, turning)
		- or at least precompute this for realistic headways
- randomized delays and train backups/queues
- schedule switching (weekend/late-night/etc.)
- incorporate other transit modes (e.g. bus)
