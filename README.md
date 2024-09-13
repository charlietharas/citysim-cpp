Necessary short term improvements:
- substantially improve pathfinding performance
	- increase cache utilization & performance (remove duplication, change placement/retrieval algorithm, restrict caching to complex paths)
	- multithread pathfinding? either internally (multithreaded bidirectional) or externally (several pathfinders active simultaneously)
	- path contraction (to POIs/line nodes) so long as it improves performance
- substantially improve citizen update loop performance
	- delay updates/checks on citizens that aren't expected to get to their destination for a while (some sort of priority queue/other data structure)
	- contract citizen paths post-generation
	- explore options for vectorization/individual update optimization

Possibilities for extension:
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
