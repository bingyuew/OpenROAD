# gcd_nangate45 IO placement
source "helpers.tcl"
read_lef Nangate45/Nangate45.lef
read_def large_groups1.def

set_io_pin_constraint -region top:* -group -order -pin_names {pin0 pin1 pin2 pin3 pin4 pin5 pin6 pin7 pin8 pin9 pin10 pin11
pin12 pin13 pin14 pin15 pin16 pin17 pin18 pin19 pin20 pin21 pin22
pin23 pin24 pin25 pin26 pin27 pin28 pin29 pin30 pin31 pin32 pin33
pin34 pin35 pin36 pin37 pin38 pin39 pin40 pin41 pin42 pin43 pin44
pin45 pin46 pin47 pin48 pin49 pin50 pin51 pin52 pin53 pin54 pin55
pin56 pin57 pin58 pin59 pin60 pin61 pin62 pin63 pin64 pin65 pin66
pin67 pin68 pin69 pin70 pin71 pin72 pin73 pin74 pin75 pin76 pin77
pin78 pin79 pin80 pin81 pin82 pin83 pin84 pin85 pin86 pin87 pin88
pin89 pin90 pin91 pin92 pin93 pin94 pin95 pin96 pin97 pin98 pin99
pin100 pin101 pin102 pin103 pin104 pin105 pin106 pin107 pin108 pin109
pin110 pin111 pin112 pin113 pin114 pin115 pin116 pin117 pin118 pin119
pin120 pin121 pin122 pin123 pin124 pin125 pin126 pin127 pin128 pin129
pin130 pin131 pin132 pin133 pin134 pin135 pin136 pin137 pin138 pin139
pin140 pin141 pin142 pin143 pin144 pin145 pin146 pin147 pin148 pin149
pin150 pin151 pin152 pin153 pin154 pin155 pin156 pin157 pin158 pin159
pin160 pin161 pin162 pin163 pin164 pin165 pin166 pin167 pin168 pin169
pin170 pin171 pin172 pin173 pin174 pin175 pin176 pin177 pin178 pin179
pin180 pin181 pin182 pin183 pin184 pin185 pin186 pin187 pin188 pin189
pin190 pin191 pin192 pin193 pin194 pin195 pin196 pin197 pin198 pin199
pin200 pin201 pin202 pin203 pin204 pin205 pin206 pin207 pin208 pin209
pin210 pin211 pin212 pin213 pin214 pin215 pin216 pin217 pin218 pin219
pin220 pin221 pin222 pin223 pin224 pin225 pin226 pin227 pin228 pin229
pin230 pin231 pin232 pin233 pin234 pin235 pin236 pin237 pin238 pin239
pin240 pin241 pin242 pin243 pin244 pin245 pin246 pin247 pin248 pin249
pin250 pin251 pin252 pin253 pin254 pin255 pin256 pin257 pin258 pin259
pin260 pin261 pin262 pin263 pin264 pin265
pin266 pin267 pin268 pin269 pin270 pin271 pin272 pin273 pin274 pin275
pin276 pin277 pin278 pin279 pin280 pin281 pin282 pin283 pin284 pin285
pin286 pin287 pin288 pin289 pin290 pin291 pin292 pin293 pin294 pin295
pin296 pin297 pin298 pin299}

set_io_pin_constraint -region top:* -group -order -pin_names { pin300 pin301 pin302 pin303 pin304 pin305
pin306 pin307 pin308 pin309 pin310 pin311 pin312 pin313 pin314 pin315
pin316 pin317 pin318 pin319 pin320 pin321 pin322 pin323 pin324 pin325
pin326 pin327 pin328 pin329 pin330 pin331 pin332 pin333 pin334 pin335
pin336 pin337 pin338 pin339 pin340 pin341 pin342 pin343 pin344 pin345
pin346 pin347 pin348 pin349 pin350 pin351 pin352 pin353 pin354 pin355
pin356 pin357 pin358 pin359 pin360 pin361 pin362 pin363 pin364 pin365
pin366 pin367 pin368 pin369 pin370 }

place_pins -hor_layers metal3 -ver_layers metal2 -corner_avoidance 15 -min_distance 0.12

set def_file [make_result_file large_groups3.def]

write_def $def_file

diff_file large_groups3.defok $def_file