<?php
function myecho($str)
{
	$file = dirname(__FILE__);
	$handle = fopen($file . "\\..\\temp\\output.txt", "a+");
	if ($handle == true)
	{
		fwrite($handle, $str);
		fclose($handle);
	}
	else
	{
		echo "Failed opening file within folder $file.\n";
	}
}

function logToFile($str)
{
	$file = dirname(__FILE__);
	$handle = fopen($file . "\\..\\temp\\result.txt", "a+");
	if ($handle == true)
	{
		fwrite($handle, $str);
		fclose($handle);
	}
	else
	{
		echo "Failed opening file within folder $file.\n";
	}
}
?>