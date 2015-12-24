--TEST--
Wincache - Testing wincache_ucache_* functions with nested arrays as a value
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

echo("setting array\n");
/*
 * NOTE: setting ucache entries via an array, if the value is not defined, then
 * the index is not added to the cache.  So, 'yellow' below won't be added to
 * the WinCache ucache.
 */
$frag1 = array('green' => 13, 'black' => 14);
$bar1 = array('green' => 5, 'Blue' => '6', 'yellow', 'cyan' => 'eight');
$bar2 = array('greenish' => 9, 'Blueish' => '10', 'yellowish', 'cyanish' => 'twelve', 'blackish' => $frag1);
$bar3 = array('greenlike' => 9, 'pink' => 10, 'chartruce' => '11', 'brown', 'magenta' => 'thirteen');
$master = array('first' => $bar1, 'second' => $bar2, 'third' => $bar3, 'fourth' => $bar1, 'fifth' => $bar2, '6th' => $bar2);


var_dump(wincache_ucache_add('master', $master));
var_dump(wincache_ucache_exists('master'));
var_dump(wincache_ucache_get('master'));
var_dump(wincache_ucache_info(false, 'master'));
var_dump(wincache_ucache_delete('master'));

echo("clearing ucache\n");
var_dump(wincache_ucache_clear());

echo("Done!");

?>
--EXPECTF--
clearing ucache
bool(true)
setting array
bool(true)
bool(true)
array(6) {
  ["first"]=>
  array(4) {
    ["green"]=>
    int(5)
    ["Blue"]=>
    string(1) "6"
    [0]=>
    string(6) "yellow"
    ["cyan"]=>
    string(5) "eight"
  }
  ["second"]=>
  array(5) {
    ["greenish"]=>
    int(9)
    ["Blueish"]=>
    string(2) "10"
    [0]=>
    string(9) "yellowish"
    ["cyanish"]=>
    string(6) "twelve"
    ["blackish"]=>
    array(2) {
      ["green"]=>
      int(13)
      ["black"]=>
      int(14)
    }
  }
  ["third"]=>
  array(5) {
    ["greenlike"]=>
    int(9)
    ["pink"]=>
    int(10)
    ["chartruce"]=>
    string(2) "11"
    [0]=>
    string(5) "brown"
    ["magenta"]=>
    string(8) "thirteen"
  }
  ["fourth"]=>
  array(4) {
    ["green"]=>
    int(5)
    ["Blue"]=>
    string(1) "6"
    [0]=>
    string(6) "yellow"
    ["cyan"]=>
    string(5) "eight"
  }
  ["fifth"]=>
  array(5) {
    ["greenish"]=>
    int(9)
    ["Blueish"]=>
    string(2) "10"
    [0]=>
    string(9) "yellowish"
    ["cyanish"]=>
    string(6) "twelve"
    ["blackish"]=>
    array(2) {
      ["green"]=>
      int(13)
      ["black"]=>
      int(14)
    }
  }
  ["6th"]=>
  array(5) {
    ["greenish"]=>
    int(9)
    ["Blueish"]=>
    string(2) "10"
    [0]=>
    string(9) "yellowish"
    ["cyanish"]=>
    string(6) "twelve"
    ["blackish"]=>
    array(2) {
      ["green"]=>
      int(13)
      ["black"]=>
      int(14)
    }
  }
}
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
      string(6) "master"
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
bool(true)
clearing ucache
bool(true)
Done!