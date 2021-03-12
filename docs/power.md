# Electricity network

Cleaned network data in `data/electricity` is is geopackages named by three
letter country code. For example, for Vietnam:

`VNM_network.gpkg` contains the power network as nodes:

- Layer name: nodes
- Geometry: Point
- id: String
- source_id: String
- type: String

and edges:

- Layer name: edges
- Geometry: Line String
- id: String
- source_id: String
- type: String
- from_id: String
- to_id: String
- length_m: Real


`VNM_plants.gpkg` repeats the nodes of type "source" - these are extracted from
the WRI power plants database, and includes details of capacity, primary fuel
and estimated generation (where available):

- Layer name: VNM_plants
- Geometry: Point
- source_id: String
- name: String
- capacity_mw: Real
- estimated_generation_gwh: Real
- primary_fuel: String
- type: String
- id: String

`VNM_targets.gpkg` repeats the nodes of type "target" - these are the target
areas (represented by their centroid in the full network) extracted from
GridFinder, with population and GDP attributed:

- Layer name: VNM_targets
- Geometry: Polygon
- area_km2: Real
- population: Real
- population_density_at_centroid: Real
- gdp_pc: Real
- gdp: Real
- type: String
- id: String
