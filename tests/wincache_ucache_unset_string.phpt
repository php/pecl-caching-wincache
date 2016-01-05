--TEST--
Wincache - unset string data fetched from user cache
--SKIPIF--
<?php include('skipif.inc'); ?>
--INI--
wincache.enablecli=1
wincache.ucenabled=1
--FILE--
<?php

$tags = array(array("name" => "page_cache_response_policy"), array("name" => "dynamic_page_response_policy"));
$cid = 'cacheid';

wincache_ucache_set($cid, $tags);
$data = wincache_ucache_get($cid);

$tagdef1 = serialize($data);

foreach ($data as $tag_key => $tag){
    $name = $tag['name'];
	unset($tag['name']);
	$tagdef2 = serialize($data);
    
    if ($tagdef1 != $tagdef2) {
      // By some sort of magic the $data array was modified, and what is 
      // worse, is now populated with some content that I don't know where it
      // came from.
      echo('This is bad.');
      exit();
    }
}
?>
==DONE==
--EXPECTF--
==DONE==