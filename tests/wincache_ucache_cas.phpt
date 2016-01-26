--TEST--
Wincache - Testing wincache_ucache_cas function
--SKIPIF--
<?php include('skipif.inc'); ?>
--INI--
wincache.enablecli=1
wincache.ucenabled=1
--FILE--
<?php
wincache_ucache_set('counter', 2922);
var_dump(wincache_ucache_cas('counter', 2922, 1));
var_dump(wincache_ucache_get('counter'));
?>
==DONE==
--EXPECTF--
bool(true)
int(1)
==DONE==
