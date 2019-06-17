--TEST--
Wincache - Testing wincache_ucache_* functions with a large array as a value
--SKIPIF--
<?php include('skipif.inc'); ?>
--INI--
wincache.enablecli=1
wincache.fcenabled=1
wincache.ucenabled=1
--FILE--
<?php

echo("clearing ucache\n");
var_dump(wincache_ucache_clear());

echo("setting large array\n");
$large_array = array('root');
for ($i = 1; $i < 511; $i++)
{
    $large_array[$i] = "element " . $i;
}

echo("count: " . count($large_array) . "\n");

echo("adding\n");
var_dump(wincache_ucache_add('large_array', $large_array));
echo("exists\n");
var_dump(wincache_ucache_exists('large_array'));
echo("get\n");
var_dump(wincache_ucache_get('large_array'));
echo("info\n");
var_dump(wincache_ucache_info(false, 'large_array'));
echo("delete\n");
var_dump(wincache_ucache_delete('large_array'));
echo("get after delete\n");
var_dump(wincache_ucache_get('large_array'));

echo("clearing ucache\n");
var_dump(wincache_ucache_clear());

echo("Done!");

?>
--EXPECTF--
clearing ucache
bool(true)
setting large array
count: 511
adding
bool(true)
exists
bool(true)
get
array(511) {
  [0]=>
  string(4) "root"
  [1]=>
  string(9) "element 1"
  [2]=>
  string(9) "element 2"
  [3]=>
  string(9) "element 3"
  [4]=>
  string(9) "element 4"
  [5]=>
  string(9) "element 5"
  [6]=>
  string(9) "element 6"
  [7]=>
  string(9) "element 7"
  [8]=>
  string(9) "element 8"
  [9]=>
  string(9) "element 9"
  [10]=>
  string(10) "element 10"
  [11]=>
  string(10) "element 11"
  [12]=>
  string(10) "element 12"
  [13]=>
  string(10) "element 13"
  [14]=>
  string(10) "element 14"
  [15]=>
  string(10) "element 15"
  [16]=>
  string(10) "element 16"
  [17]=>
  string(10) "element 17"
  [18]=>
  string(10) "element 18"
  [19]=>
  string(10) "element 19"
  [20]=>
  string(10) "element 20"
  [21]=>
  string(10) "element 21"
  [22]=>
  string(10) "element 22"
  [23]=>
  string(10) "element 23"
  [24]=>
  string(10) "element 24"
  [25]=>
  string(10) "element 25"
  [26]=>
  string(10) "element 26"
  [27]=>
  string(10) "element 27"
  [28]=>
  string(10) "element 28"
  [29]=>
  string(10) "element 29"
  [30]=>
  string(10) "element 30"
  [31]=>
  string(10) "element 31"
  [32]=>
  string(10) "element 32"
  [33]=>
  string(10) "element 33"
  [34]=>
  string(10) "element 34"
  [35]=>
  string(10) "element 35"
  [36]=>
  string(10) "element 36"
  [37]=>
  string(10) "element 37"
  [38]=>
  string(10) "element 38"
  [39]=>
  string(10) "element 39"
  [40]=>
  string(10) "element 40"
  [41]=>
  string(10) "element 41"
  [42]=>
  string(10) "element 42"
  [43]=>
  string(10) "element 43"
  [44]=>
  string(10) "element 44"
  [45]=>
  string(10) "element 45"
  [46]=>
  string(10) "element 46"
  [47]=>
  string(10) "element 47"
  [48]=>
  string(10) "element 48"
  [49]=>
  string(10) "element 49"
  [50]=>
  string(10) "element 50"
  [51]=>
  string(10) "element 51"
  [52]=>
  string(10) "element 52"
  [53]=>
  string(10) "element 53"
  [54]=>
  string(10) "element 54"
  [55]=>
  string(10) "element 55"
  [56]=>
  string(10) "element 56"
  [57]=>
  string(10) "element 57"
  [58]=>
  string(10) "element 58"
  [59]=>
  string(10) "element 59"
  [60]=>
  string(10) "element 60"
  [61]=>
  string(10) "element 61"
  [62]=>
  string(10) "element 62"
  [63]=>
  string(10) "element 63"
  [64]=>
  string(10) "element 64"
  [65]=>
  string(10) "element 65"
  [66]=>
  string(10) "element 66"
  [67]=>
  string(10) "element 67"
  [68]=>
  string(10) "element 68"
  [69]=>
  string(10) "element 69"
  [70]=>
  string(10) "element 70"
  [71]=>
  string(10) "element 71"
  [72]=>
  string(10) "element 72"
  [73]=>
  string(10) "element 73"
  [74]=>
  string(10) "element 74"
  [75]=>
  string(10) "element 75"
  [76]=>
  string(10) "element 76"
  [77]=>
  string(10) "element 77"
  [78]=>
  string(10) "element 78"
  [79]=>
  string(10) "element 79"
  [80]=>
  string(10) "element 80"
  [81]=>
  string(10) "element 81"
  [82]=>
  string(10) "element 82"
  [83]=>
  string(10) "element 83"
  [84]=>
  string(10) "element 84"
  [85]=>
  string(10) "element 85"
  [86]=>
  string(10) "element 86"
  [87]=>
  string(10) "element 87"
  [88]=>
  string(10) "element 88"
  [89]=>
  string(10) "element 89"
  [90]=>
  string(10) "element 90"
  [91]=>
  string(10) "element 91"
  [92]=>
  string(10) "element 92"
  [93]=>
  string(10) "element 93"
  [94]=>
  string(10) "element 94"
  [95]=>
  string(10) "element 95"
  [96]=>
  string(10) "element 96"
  [97]=>
  string(10) "element 97"
  [98]=>
  string(10) "element 98"
  [99]=>
  string(10) "element 99"
  [100]=>
  string(11) "element 100"
  [101]=>
  string(11) "element 101"
  [102]=>
  string(11) "element 102"
  [103]=>
  string(11) "element 103"
  [104]=>
  string(11) "element 104"
  [105]=>
  string(11) "element 105"
  [106]=>
  string(11) "element 106"
  [107]=>
  string(11) "element 107"
  [108]=>
  string(11) "element 108"
  [109]=>
  string(11) "element 109"
  [110]=>
  string(11) "element 110"
  [111]=>
  string(11) "element 111"
  [112]=>
  string(11) "element 112"
  [113]=>
  string(11) "element 113"
  [114]=>
  string(11) "element 114"
  [115]=>
  string(11) "element 115"
  [116]=>
  string(11) "element 116"
  [117]=>
  string(11) "element 117"
  [118]=>
  string(11) "element 118"
  [119]=>
  string(11) "element 119"
  [120]=>
  string(11) "element 120"
  [121]=>
  string(11) "element 121"
  [122]=>
  string(11) "element 122"
  [123]=>
  string(11) "element 123"
  [124]=>
  string(11) "element 124"
  [125]=>
  string(11) "element 125"
  [126]=>
  string(11) "element 126"
  [127]=>
  string(11) "element 127"
  [128]=>
  string(11) "element 128"
  [129]=>
  string(11) "element 129"
  [130]=>
  string(11) "element 130"
  [131]=>
  string(11) "element 131"
  [132]=>
  string(11) "element 132"
  [133]=>
  string(11) "element 133"
  [134]=>
  string(11) "element 134"
  [135]=>
  string(11) "element 135"
  [136]=>
  string(11) "element 136"
  [137]=>
  string(11) "element 137"
  [138]=>
  string(11) "element 138"
  [139]=>
  string(11) "element 139"
  [140]=>
  string(11) "element 140"
  [141]=>
  string(11) "element 141"
  [142]=>
  string(11) "element 142"
  [143]=>
  string(11) "element 143"
  [144]=>
  string(11) "element 144"
  [145]=>
  string(11) "element 145"
  [146]=>
  string(11) "element 146"
  [147]=>
  string(11) "element 147"
  [148]=>
  string(11) "element 148"
  [149]=>
  string(11) "element 149"
  [150]=>
  string(11) "element 150"
  [151]=>
  string(11) "element 151"
  [152]=>
  string(11) "element 152"
  [153]=>
  string(11) "element 153"
  [154]=>
  string(11) "element 154"
  [155]=>
  string(11) "element 155"
  [156]=>
  string(11) "element 156"
  [157]=>
  string(11) "element 157"
  [158]=>
  string(11) "element 158"
  [159]=>
  string(11) "element 159"
  [160]=>
  string(11) "element 160"
  [161]=>
  string(11) "element 161"
  [162]=>
  string(11) "element 162"
  [163]=>
  string(11) "element 163"
  [164]=>
  string(11) "element 164"
  [165]=>
  string(11) "element 165"
  [166]=>
  string(11) "element 166"
  [167]=>
  string(11) "element 167"
  [168]=>
  string(11) "element 168"
  [169]=>
  string(11) "element 169"
  [170]=>
  string(11) "element 170"
  [171]=>
  string(11) "element 171"
  [172]=>
  string(11) "element 172"
  [173]=>
  string(11) "element 173"
  [174]=>
  string(11) "element 174"
  [175]=>
  string(11) "element 175"
  [176]=>
  string(11) "element 176"
  [177]=>
  string(11) "element 177"
  [178]=>
  string(11) "element 178"
  [179]=>
  string(11) "element 179"
  [180]=>
  string(11) "element 180"
  [181]=>
  string(11) "element 181"
  [182]=>
  string(11) "element 182"
  [183]=>
  string(11) "element 183"
  [184]=>
  string(11) "element 184"
  [185]=>
  string(11) "element 185"
  [186]=>
  string(11) "element 186"
  [187]=>
  string(11) "element 187"
  [188]=>
  string(11) "element 188"
  [189]=>
  string(11) "element 189"
  [190]=>
  string(11) "element 190"
  [191]=>
  string(11) "element 191"
  [192]=>
  string(11) "element 192"
  [193]=>
  string(11) "element 193"
  [194]=>
  string(11) "element 194"
  [195]=>
  string(11) "element 195"
  [196]=>
  string(11) "element 196"
  [197]=>
  string(11) "element 197"
  [198]=>
  string(11) "element 198"
  [199]=>
  string(11) "element 199"
  [200]=>
  string(11) "element 200"
  [201]=>
  string(11) "element 201"
  [202]=>
  string(11) "element 202"
  [203]=>
  string(11) "element 203"
  [204]=>
  string(11) "element 204"
  [205]=>
  string(11) "element 205"
  [206]=>
  string(11) "element 206"
  [207]=>
  string(11) "element 207"
  [208]=>
  string(11) "element 208"
  [209]=>
  string(11) "element 209"
  [210]=>
  string(11) "element 210"
  [211]=>
  string(11) "element 211"
  [212]=>
  string(11) "element 212"
  [213]=>
  string(11) "element 213"
  [214]=>
  string(11) "element 214"
  [215]=>
  string(11) "element 215"
  [216]=>
  string(11) "element 216"
  [217]=>
  string(11) "element 217"
  [218]=>
  string(11) "element 218"
  [219]=>
  string(11) "element 219"
  [220]=>
  string(11) "element 220"
  [221]=>
  string(11) "element 221"
  [222]=>
  string(11) "element 222"
  [223]=>
  string(11) "element 223"
  [224]=>
  string(11) "element 224"
  [225]=>
  string(11) "element 225"
  [226]=>
  string(11) "element 226"
  [227]=>
  string(11) "element 227"
  [228]=>
  string(11) "element 228"
  [229]=>
  string(11) "element 229"
  [230]=>
  string(11) "element 230"
  [231]=>
  string(11) "element 231"
  [232]=>
  string(11) "element 232"
  [233]=>
  string(11) "element 233"
  [234]=>
  string(11) "element 234"
  [235]=>
  string(11) "element 235"
  [236]=>
  string(11) "element 236"
  [237]=>
  string(11) "element 237"
  [238]=>
  string(11) "element 238"
  [239]=>
  string(11) "element 239"
  [240]=>
  string(11) "element 240"
  [241]=>
  string(11) "element 241"
  [242]=>
  string(11) "element 242"
  [243]=>
  string(11) "element 243"
  [244]=>
  string(11) "element 244"
  [245]=>
  string(11) "element 245"
  [246]=>
  string(11) "element 246"
  [247]=>
  string(11) "element 247"
  [248]=>
  string(11) "element 248"
  [249]=>
  string(11) "element 249"
  [250]=>
  string(11) "element 250"
  [251]=>
  string(11) "element 251"
  [252]=>
  string(11) "element 252"
  [253]=>
  string(11) "element 253"
  [254]=>
  string(11) "element 254"
  [255]=>
  string(11) "element 255"
  [256]=>
  string(11) "element 256"
  [257]=>
  string(11) "element 257"
  [258]=>
  string(11) "element 258"
  [259]=>
  string(11) "element 259"
  [260]=>
  string(11) "element 260"
  [261]=>
  string(11) "element 261"
  [262]=>
  string(11) "element 262"
  [263]=>
  string(11) "element 263"
  [264]=>
  string(11) "element 264"
  [265]=>
  string(11) "element 265"
  [266]=>
  string(11) "element 266"
  [267]=>
  string(11) "element 267"
  [268]=>
  string(11) "element 268"
  [269]=>
  string(11) "element 269"
  [270]=>
  string(11) "element 270"
  [271]=>
  string(11) "element 271"
  [272]=>
  string(11) "element 272"
  [273]=>
  string(11) "element 273"
  [274]=>
  string(11) "element 274"
  [275]=>
  string(11) "element 275"
  [276]=>
  string(11) "element 276"
  [277]=>
  string(11) "element 277"
  [278]=>
  string(11) "element 278"
  [279]=>
  string(11) "element 279"
  [280]=>
  string(11) "element 280"
  [281]=>
  string(11) "element 281"
  [282]=>
  string(11) "element 282"
  [283]=>
  string(11) "element 283"
  [284]=>
  string(11) "element 284"
  [285]=>
  string(11) "element 285"
  [286]=>
  string(11) "element 286"
  [287]=>
  string(11) "element 287"
  [288]=>
  string(11) "element 288"
  [289]=>
  string(11) "element 289"
  [290]=>
  string(11) "element 290"
  [291]=>
  string(11) "element 291"
  [292]=>
  string(11) "element 292"
  [293]=>
  string(11) "element 293"
  [294]=>
  string(11) "element 294"
  [295]=>
  string(11) "element 295"
  [296]=>
  string(11) "element 296"
  [297]=>
  string(11) "element 297"
  [298]=>
  string(11) "element 298"
  [299]=>
  string(11) "element 299"
  [300]=>
  string(11) "element 300"
  [301]=>
  string(11) "element 301"
  [302]=>
  string(11) "element 302"
  [303]=>
  string(11) "element 303"
  [304]=>
  string(11) "element 304"
  [305]=>
  string(11) "element 305"
  [306]=>
  string(11) "element 306"
  [307]=>
  string(11) "element 307"
  [308]=>
  string(11) "element 308"
  [309]=>
  string(11) "element 309"
  [310]=>
  string(11) "element 310"
  [311]=>
  string(11) "element 311"
  [312]=>
  string(11) "element 312"
  [313]=>
  string(11) "element 313"
  [314]=>
  string(11) "element 314"
  [315]=>
  string(11) "element 315"
  [316]=>
  string(11) "element 316"
  [317]=>
  string(11) "element 317"
  [318]=>
  string(11) "element 318"
  [319]=>
  string(11) "element 319"
  [320]=>
  string(11) "element 320"
  [321]=>
  string(11) "element 321"
  [322]=>
  string(11) "element 322"
  [323]=>
  string(11) "element 323"
  [324]=>
  string(11) "element 324"
  [325]=>
  string(11) "element 325"
  [326]=>
  string(11) "element 326"
  [327]=>
  string(11) "element 327"
  [328]=>
  string(11) "element 328"
  [329]=>
  string(11) "element 329"
  [330]=>
  string(11) "element 330"
  [331]=>
  string(11) "element 331"
  [332]=>
  string(11) "element 332"
  [333]=>
  string(11) "element 333"
  [334]=>
  string(11) "element 334"
  [335]=>
  string(11) "element 335"
  [336]=>
  string(11) "element 336"
  [337]=>
  string(11) "element 337"
  [338]=>
  string(11) "element 338"
  [339]=>
  string(11) "element 339"
  [340]=>
  string(11) "element 340"
  [341]=>
  string(11) "element 341"
  [342]=>
  string(11) "element 342"
  [343]=>
  string(11) "element 343"
  [344]=>
  string(11) "element 344"
  [345]=>
  string(11) "element 345"
  [346]=>
  string(11) "element 346"
  [347]=>
  string(11) "element 347"
  [348]=>
  string(11) "element 348"
  [349]=>
  string(11) "element 349"
  [350]=>
  string(11) "element 350"
  [351]=>
  string(11) "element 351"
  [352]=>
  string(11) "element 352"
  [353]=>
  string(11) "element 353"
  [354]=>
  string(11) "element 354"
  [355]=>
  string(11) "element 355"
  [356]=>
  string(11) "element 356"
  [357]=>
  string(11) "element 357"
  [358]=>
  string(11) "element 358"
  [359]=>
  string(11) "element 359"
  [360]=>
  string(11) "element 360"
  [361]=>
  string(11) "element 361"
  [362]=>
  string(11) "element 362"
  [363]=>
  string(11) "element 363"
  [364]=>
  string(11) "element 364"
  [365]=>
  string(11) "element 365"
  [366]=>
  string(11) "element 366"
  [367]=>
  string(11) "element 367"
  [368]=>
  string(11) "element 368"
  [369]=>
  string(11) "element 369"
  [370]=>
  string(11) "element 370"
  [371]=>
  string(11) "element 371"
  [372]=>
  string(11) "element 372"
  [373]=>
  string(11) "element 373"
  [374]=>
  string(11) "element 374"
  [375]=>
  string(11) "element 375"
  [376]=>
  string(11) "element 376"
  [377]=>
  string(11) "element 377"
  [378]=>
  string(11) "element 378"
  [379]=>
  string(11) "element 379"
  [380]=>
  string(11) "element 380"
  [381]=>
  string(11) "element 381"
  [382]=>
  string(11) "element 382"
  [383]=>
  string(11) "element 383"
  [384]=>
  string(11) "element 384"
  [385]=>
  string(11) "element 385"
  [386]=>
  string(11) "element 386"
  [387]=>
  string(11) "element 387"
  [388]=>
  string(11) "element 388"
  [389]=>
  string(11) "element 389"
  [390]=>
  string(11) "element 390"
  [391]=>
  string(11) "element 391"
  [392]=>
  string(11) "element 392"
  [393]=>
  string(11) "element 393"
  [394]=>
  string(11) "element 394"
  [395]=>
  string(11) "element 395"
  [396]=>
  string(11) "element 396"
  [397]=>
  string(11) "element 397"
  [398]=>
  string(11) "element 398"
  [399]=>
  string(11) "element 399"
  [400]=>
  string(11) "element 400"
  [401]=>
  string(11) "element 401"
  [402]=>
  string(11) "element 402"
  [403]=>
  string(11) "element 403"
  [404]=>
  string(11) "element 404"
  [405]=>
  string(11) "element 405"
  [406]=>
  string(11) "element 406"
  [407]=>
  string(11) "element 407"
  [408]=>
  string(11) "element 408"
  [409]=>
  string(11) "element 409"
  [410]=>
  string(11) "element 410"
  [411]=>
  string(11) "element 411"
  [412]=>
  string(11) "element 412"
  [413]=>
  string(11) "element 413"
  [414]=>
  string(11) "element 414"
  [415]=>
  string(11) "element 415"
  [416]=>
  string(11) "element 416"
  [417]=>
  string(11) "element 417"
  [418]=>
  string(11) "element 418"
  [419]=>
  string(11) "element 419"
  [420]=>
  string(11) "element 420"
  [421]=>
  string(11) "element 421"
  [422]=>
  string(11) "element 422"
  [423]=>
  string(11) "element 423"
  [424]=>
  string(11) "element 424"
  [425]=>
  string(11) "element 425"
  [426]=>
  string(11) "element 426"
  [427]=>
  string(11) "element 427"
  [428]=>
  string(11) "element 428"
  [429]=>
  string(11) "element 429"
  [430]=>
  string(11) "element 430"
  [431]=>
  string(11) "element 431"
  [432]=>
  string(11) "element 432"
  [433]=>
  string(11) "element 433"
  [434]=>
  string(11) "element 434"
  [435]=>
  string(11) "element 435"
  [436]=>
  string(11) "element 436"
  [437]=>
  string(11) "element 437"
  [438]=>
  string(11) "element 438"
  [439]=>
  string(11) "element 439"
  [440]=>
  string(11) "element 440"
  [441]=>
  string(11) "element 441"
  [442]=>
  string(11) "element 442"
  [443]=>
  string(11) "element 443"
  [444]=>
  string(11) "element 444"
  [445]=>
  string(11) "element 445"
  [446]=>
  string(11) "element 446"
  [447]=>
  string(11) "element 447"
  [448]=>
  string(11) "element 448"
  [449]=>
  string(11) "element 449"
  [450]=>
  string(11) "element 450"
  [451]=>
  string(11) "element 451"
  [452]=>
  string(11) "element 452"
  [453]=>
  string(11) "element 453"
  [454]=>
  string(11) "element 454"
  [455]=>
  string(11) "element 455"
  [456]=>
  string(11) "element 456"
  [457]=>
  string(11) "element 457"
  [458]=>
  string(11) "element 458"
  [459]=>
  string(11) "element 459"
  [460]=>
  string(11) "element 460"
  [461]=>
  string(11) "element 461"
  [462]=>
  string(11) "element 462"
  [463]=>
  string(11) "element 463"
  [464]=>
  string(11) "element 464"
  [465]=>
  string(11) "element 465"
  [466]=>
  string(11) "element 466"
  [467]=>
  string(11) "element 467"
  [468]=>
  string(11) "element 468"
  [469]=>
  string(11) "element 469"
  [470]=>
  string(11) "element 470"
  [471]=>
  string(11) "element 471"
  [472]=>
  string(11) "element 472"
  [473]=>
  string(11) "element 473"
  [474]=>
  string(11) "element 474"
  [475]=>
  string(11) "element 475"
  [476]=>
  string(11) "element 476"
  [477]=>
  string(11) "element 477"
  [478]=>
  string(11) "element 478"
  [479]=>
  string(11) "element 479"
  [480]=>
  string(11) "element 480"
  [481]=>
  string(11) "element 481"
  [482]=>
  string(11) "element 482"
  [483]=>
  string(11) "element 483"
  [484]=>
  string(11) "element 484"
  [485]=>
  string(11) "element 485"
  [486]=>
  string(11) "element 486"
  [487]=>
  string(11) "element 487"
  [488]=>
  string(11) "element 488"
  [489]=>
  string(11) "element 489"
  [490]=>
  string(11) "element 490"
  [491]=>
  string(11) "element 491"
  [492]=>
  string(11) "element 492"
  [493]=>
  string(11) "element 493"
  [494]=>
  string(11) "element 494"
  [495]=>
  string(11) "element 495"
  [496]=>
  string(11) "element 496"
  [497]=>
  string(11) "element 497"
  [498]=>
  string(11) "element 498"
  [499]=>
  string(11) "element 499"
  [500]=>
  string(11) "element 500"
  [501]=>
  string(11) "element 501"
  [502]=>
  string(11) "element 502"
  [503]=>
  string(11) "element 503"
  [504]=>
  string(11) "element 504"
  [505]=>
  string(11) "element 505"
  [506]=>
  string(11) "element 506"
  [507]=>
  string(11) "element 507"
  [508]=>
  string(11) "element 508"
  [509]=>
  string(11) "element 509"
  [510]=>
  string(11) "element 510"
}
info
array(6) {
  ["total_cache_uptime"]=>
  int(0)
  ["is_local_cache"]=>
  bool(false)
  ["total_item_count"]=>
  int(1)
  ["total_hit_count"]=>
  int(1)
  ["total_miss_count"]=>
  int(0)
  ["ucache_entries"]=>
  array(1) {
    [1]=>
    array(6) {
      ["key_name"]=>
      string(11) "large_array"
      ["value_type"]=>
      string(5) "array"
      ["value_size"]=>
      int(%d)
      ["ttl_seconds"]=>
      int(0)
      ["age_seconds"]=>
      int(0)
      ["hitcount"]=>
      int(1)
    }
  }
}
delete
bool(true)
get after delete
bool(false)
clearing ucache
bool(true)
Done!