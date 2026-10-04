# Gaussian cache v1

All integers/floats are little-endian. Header is 8 ASCII bytes `GZGS0001`
followed by a uint32 record count (1–2,000,000). Exact file length is
`12 + 236 * count`. Trailing bytes, missing records and unknown magic fail.

Each packed 236-byte record contains:

| Offset | Values | Meaning |
| --- | --- | --- |
| 0 | 3 float32 | world-space center xyz |
| 12 | 3 float32 | positive Gaussian standard deviations xyz |
| 24 | 4 float32 | quaternion xyzw |
| 40 | 48 float32 | RGB spherical harmonics, coefficient-major degree 3 |
| 232 | 4 uint8 | RGB diagnostic colour and sigmoid opacity alpha |

SH values are `[c0.r,c0.g,c0.b,c1.r,...,c15.b]`. Degree-0 display colour is
`0.5 + 0.28209479177387814 * c0`. RGB bytes are diagnostic; the shader evaluates
SH for colour. Input quaternions must be nonzero and finite; converter outputs
unit quaternions. The native loader validates before allocating GPU resources.

The cache is independent of training sessions and mission identities. The old
internal `ARCHGS02` cache is intentionally not accepted; convert its source PLY.
Future incompatible formats must receive a different magic/version.
