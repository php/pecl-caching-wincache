<?php
require_once 'common.inc';
logToFile("Testing ini directive include_path: ");
ini_set("include_path", ".;foo");
include 'wincache_base7_1.php';
ini_set("include_path", ".;bar");
include 'wincache_base7_1.php';
?> 