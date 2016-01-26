--TEST--
Wincache - Testing wincache_ucache_[inc|dec] functions
--SKIPIF--
<?php include('skipif.inc'); ?>
--INI--
wincache.enablecli=1
wincache.fcenabled=1
wincache.ucenabled=1
--FILE--
<?php
$key = 'mykey';
$success = FALSE;

wincache_ucache_delete($key);
wincache_ucache_inc($key, 1, $success);
var_dump($success);
wincache_ucache_set($key, 0);
wincache_ucache_inc($key, 1, $success);
var_dump($success);
wincache_ucache_dec($key, 1, $success);
var_dump($success);
?>
==DONE==
--EXPECTF--
bool(false)
bool(true)
bool(true)
==DONE==
