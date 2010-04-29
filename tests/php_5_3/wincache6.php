<?php
require 'common.inc';
require_once 'lib/wincache_base6.php';
logToFile("Testing __callStatic function: ");
myecho(CallStatic::mytestfunction(array('key' => 'value'))->key . "\r\n");
?>