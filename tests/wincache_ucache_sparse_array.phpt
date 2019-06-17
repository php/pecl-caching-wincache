--TEST--
Wincache - Testing wincache_ucache_* functions with a sparse array as a value
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
$sparse_array = array_fill(0, 511, "fill value");
for ($i = 4; $i < 511; $i++)
{
    // Leave only the prime indexes
    for ($j = 2; $j < 19; $j++)
    {
        if (($i != $j) && (($i % $j) == 0))
        {
            unset($sparse_array[$i]);
            break;
        }
    }
}

echo("count: " . count($sparse_array) . "\n");

echo("adding\n");
var_dump(wincache_ucache_add('sparse_array', $sparse_array));
echo("exists\n");
var_dump(wincache_ucache_exists('sparse_array'));
echo("get\n");
var_dump(wincache_ucache_get('sparse_array'));
echo("info\n");
var_dump(wincache_ucache_info(false, 'sparse_array'));
echo("delete\n");
var_dump(wincache_ucache_delete('sparse_array'));
echo("get after delete\n");
var_dump(wincache_ucache_get('sparse_array'));

echo("clearing ucache\n");
var_dump(wincache_ucache_clear());

echo("Done!");

?>
--EXPECTF--
clearing ucache
bool(true)
setting large array
count: 101
adding
bool(true)
exists
bool(true)
get
array(101) {
  [0]=>
  string(10) "fill value"
  [1]=>
  string(10) "fill value"
  [2]=>
  string(10) "fill value"
  [3]=>
  string(10) "fill value"
  [5]=>
  string(10) "fill value"
  [7]=>
  string(10) "fill value"
  [11]=>
  string(10) "fill value"
  [13]=>
  string(10) "fill value"
  [17]=>
  string(10) "fill value"
  [19]=>
  string(10) "fill value"
  [23]=>
  string(10) "fill value"
  [29]=>
  string(10) "fill value"
  [31]=>
  string(10) "fill value"
  [37]=>
  string(10) "fill value"
  [41]=>
  string(10) "fill value"
  [43]=>
  string(10) "fill value"
  [47]=>
  string(10) "fill value"
  [53]=>
  string(10) "fill value"
  [59]=>
  string(10) "fill value"
  [61]=>
  string(10) "fill value"
  [67]=>
  string(10) "fill value"
  [71]=>
  string(10) "fill value"
  [73]=>
  string(10) "fill value"
  [79]=>
  string(10) "fill value"
  [83]=>
  string(10) "fill value"
  [89]=>
  string(10) "fill value"
  [97]=>
  string(10) "fill value"
  [101]=>
  string(10) "fill value"
  [103]=>
  string(10) "fill value"
  [107]=>
  string(10) "fill value"
  [109]=>
  string(10) "fill value"
  [113]=>
  string(10) "fill value"
  [127]=>
  string(10) "fill value"
  [131]=>
  string(10) "fill value"
  [137]=>
  string(10) "fill value"
  [139]=>
  string(10) "fill value"
  [149]=>
  string(10) "fill value"
  [151]=>
  string(10) "fill value"
  [157]=>
  string(10) "fill value"
  [163]=>
  string(10) "fill value"
  [167]=>
  string(10) "fill value"
  [173]=>
  string(10) "fill value"
  [179]=>
  string(10) "fill value"
  [181]=>
  string(10) "fill value"
  [191]=>
  string(10) "fill value"
  [193]=>
  string(10) "fill value"
  [197]=>
  string(10) "fill value"
  [199]=>
  string(10) "fill value"
  [211]=>
  string(10) "fill value"
  [223]=>
  string(10) "fill value"
  [227]=>
  string(10) "fill value"
  [229]=>
  string(10) "fill value"
  [233]=>
  string(10) "fill value"
  [239]=>
  string(10) "fill value"
  [241]=>
  string(10) "fill value"
  [251]=>
  string(10) "fill value"
  [257]=>
  string(10) "fill value"
  [263]=>
  string(10) "fill value"
  [269]=>
  string(10) "fill value"
  [271]=>
  string(10) "fill value"
  [277]=>
  string(10) "fill value"
  [281]=>
  string(10) "fill value"
  [283]=>
  string(10) "fill value"
  [293]=>
  string(10) "fill value"
  [307]=>
  string(10) "fill value"
  [311]=>
  string(10) "fill value"
  [313]=>
  string(10) "fill value"
  [317]=>
  string(10) "fill value"
  [331]=>
  string(10) "fill value"
  [337]=>
  string(10) "fill value"
  [347]=>
  string(10) "fill value"
  [349]=>
  string(10) "fill value"
  [353]=>
  string(10) "fill value"
  [359]=>
  string(10) "fill value"
  [361]=>
  string(10) "fill value"
  [367]=>
  string(10) "fill value"
  [373]=>
  string(10) "fill value"
  [379]=>
  string(10) "fill value"
  [383]=>
  string(10) "fill value"
  [389]=>
  string(10) "fill value"
  [397]=>
  string(10) "fill value"
  [401]=>
  string(10) "fill value"
  [409]=>
  string(10) "fill value"
  [419]=>
  string(10) "fill value"
  [421]=>
  string(10) "fill value"
  [431]=>
  string(10) "fill value"
  [433]=>
  string(10) "fill value"
  [437]=>
  string(10) "fill value"
  [439]=>
  string(10) "fill value"
  [443]=>
  string(10) "fill value"
  [449]=>
  string(10) "fill value"
  [457]=>
  string(10) "fill value"
  [461]=>
  string(10) "fill value"
  [463]=>
  string(10) "fill value"
  [467]=>
  string(10) "fill value"
  [479]=>
  string(10) "fill value"
  [487]=>
  string(10) "fill value"
  [491]=>
  string(10) "fill value"
  [499]=>
  string(10) "fill value"
  [503]=>
  string(10) "fill value"
  [509]=>
  string(10) "fill value"
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
      string(12) "sparse_array"
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