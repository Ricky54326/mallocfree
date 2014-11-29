Authors: Riccardo <riccardo> Mutschlechner, Mitchell <lutzke> Lutzke
Malloc() and free(), essentially;

WKLD1: No need for headers, simply used a bitmap to allocate/keep track of chunks
WKLD2: Still used bitmap, but with 2 extra bits denoting size (00 = 16, 01 = 80, 1x = 256)
WLKD3: Used header nodes as small as possible; coalesce entire list on every free to maximally compact.
