# Collada (.dae) exporter for Houdini

As of right now all input geometry has to be triangulated. You can do this via the "Divide" node.

## Attributes
The exporter node recognizes the following attributes:
| Attribute         | Type   | Domain          | Description                                                |
|-------------------|--------|-----------------|------------------------------------------------------------|
| name              | string | primitive       | Splits the geometry into multiple groups and sets the name |
| shop_materialpath | string | primitive       | The material of the geometry                               |
| uv                | float3 | point or vertex | The uv coordinates of the geometry                         |
| normal            | float3 | point or vertex | The normal direction of the geometry                       |
