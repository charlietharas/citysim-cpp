Simulates transit riders in the New York City subway based on annual station ridership data.

### Data Sources
https://github.com/CityOfNewYork/nyc-geo-metadata
- [Station points data](https://data.cityofnewyork.us/Transportation/Subway-Stations/arq3-7z49)
- [Line points data](https://data.cityofnewyork.us/Transportation/Subway-Lines/3qz8-muuu)

[Line order data](https://new.mta.info/maps/subway-line-maps)

[Station ridership data](https://new.mta.info/agency/new-york-city-transit/subway-bus-ridership-2021)

### Future improvements
- citizen update loop and data structure optimization
	- simulation currently broken
	- fix shitty pointer architecture
- refactor to SDL2
- fix patch caching
- fix debug report
- visual analytics/ridership dashboard
- path contraction for pathfinding algorithm
- memory optimization for stored path
- review parallelism structure
- train physics (including geoline objects and collision avoidance)
- multi pathfinding (e.g. multiple starts, multiple destinations, shortest overall path)
- dynamic train schedules
- dynamic train paths (e.g. nightly schedule)
- fix benchmark mode

More datasets to use:
- https://data.ny.gov/browse?Dataset-Information_Agency=Metropolitan+Transportation+Authority&sortBy=relevance&page=1&pageSize=20
- https://data.ny.gov/browse?tags=origin-destination
- https://www.mta.info/article/celebrating-2024-mta-open-data-challenge
- https://www.mta.info/open-data
