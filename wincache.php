<?php
/*
   +----------------------------------------------------------------------------------------------+
   | Windows Cache for PHP                                                                        |
   +----------------------------------------------------------------------------------------------+
   | Copyright (c) 2009, Microsoft Corporation. All rights reserved.                              |
   |                                                                                              |
   | Redistribution and use in source and binary forms, with or without modification, are         |
   | permitted provided that the following conditions are met:                                    |
   | - Redistributions of source code must retain the above copyright notice, this list of        |
   | conditions and the following disclaimer.                                                     |
   | - Redistributions in binary form must reproduce the above copyright notice, this list of     |
   | conditions and the following disclaimer in the documentation and/or other materials provided |
   | with the distribution.                                                                       |
   | - Neither the name of the Microsoft Corporation nor the names of its contributors may be     |
   | used to endorse or promote products derived from this software without specific prior written|
   | permission.                                                                                  |
   |                                                                                              |
   | THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS  |
   | OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF              |
   | MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE   |
   | COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,    |
   | EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE|
   | GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED   |
   | AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING    |
   | NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED |
   | OF THE POSSIBILITY OF SUCH DAMAGE.                                                           |
   +----------------------------------------------------------------------------------------------+
   | Module: wincache.php                                                                         |
   +----------------------------------------------------------------------------------------------+
   | Authors: Don Venkat Raman <don.raman@microsoft.com>                                          |
   |          Ruslan Yakushev <ruslany@microsoft.com>                                             |
   +----------------------------------------------------------------------------------------------+
*/

/**
 * ======================== CONFIGURATION SETTINGS ==============================
 * If you do not want to use authentication for this page, set USE_AUTHENTICATION to 0.
 * If you use authentication then replace the default password.
 */
define('USE_AUTHENTICATION', 1);
define('USERNAME', 'wincache');
define('PASSWORD', 'wincache');

/*The Basic PHP authentication will work only when IIS is configured to support 
'Anonymous Authentication' and nothing else. If IIS is configured to support/use
any other kind of authentication like Basic/Negotiate/Digest etc. this will not work.
In that case please define the name of users in the array below which you would like
to grant access in your domain/network/workgroup.*/
$user_allowed = array('DOMAIN\user1', 'DOMAIN\user2', 'DOMAIN\user3');

/*If the array contains string 'all' all the users authenticated by IIS
will have access to the page. Uncomment the below line and comment above line
to grant access to all users who gets authenticated by IIS.*/
/*$user_allowed = array('all');*/

/** ===================== END OF CONFIGURATION SETTINGS ========================== */

if ( !extension_loaded( 'wincache' ) )
{
    die('The extension WINCACHE (php_wincache.dll) is not loaded. No statistics to show.');
}

if ( USE_AUTHENTICATION == 1 ) {
	if (!empty($_SERVER['AUTH_TYPE']) && !empty($_SERVER['REMOTE_USER']) && strcasecmp($_SERVER['REMOTE_USER'], 'anonymous'))
	{
		if (!in_array(strtolower($_SERVER['REMOTE_USER']), array_map('strtolower', $user_allowed))
		&& !in_array('all', array_map('strtolower', $user_allowed)))
		{
			echo 'You are not authorised to view this page. Please contact server admin to get permission. Exiting.';
			exit;
		}
	}
    else if ( !isset($_SERVER['PHP_AUTH_USER'] ) || !isset( $_SERVER['PHP_AUTH_PW'] ) ||	
    $_SERVER['PHP_AUTH_USER'] != USERNAME || $_SERVER['PHP_AUTH_PW'] != PASSWORD ) {
        header( 'WWW-Authenticate: Basic realm="WINCACHE Log In!"' );
        header( 'HTTP/1.0 401 Unauthorized' );
        exit;
    }
    else if ( $_SERVER['PHP_AUTH_PW'] == 'wincache' )
    {
        echo "Please change the default password to get this page working. Exiting.";
        exit;
    }
}

define('IMG_WIDTH', 320);
define('IMG_HEIGHT', 220);
define('SUMMARY_DATA', 1);
define('OCACHE_DATA', 2);
define('FCACHE_DATA', 3);
define('RCACHE_DATA', 4);
define('PATH_MAX_LENGTH', 40);
define('INI_MAX_LENGTH', 30);
define('SUBKEY_MAX_LENGTH', 80);

// WinCache settings that are used for debugging purposes
$settings_to_hide = array( 'wincache.localheap', 'wincache.debuglevel' );

// Input parameters check
$PHP_SELF = isset( $_SERVER['PHP_SELF'] ) ? htmlentities( strip_tags( $_SERVER['PHP_SELF'],'' ), ENT_QUOTES, 'UTF-8' ) : '';
$page = isset( $_GET['page'] ) ? $_GET['page'] : SUMMARY_DATA;
if ( !is_numeric( $page ) || $page < SUMMARY_DATA || $page > RCACHE_DATA )
    $page = SUMMARY_DATA;

$img = isset( $_GET['img'] ) ? $_GET['img'] : 0;
if ( !is_numeric( $img ) || $img < OCACHE_DATA || $img > FCACHE_DATA ) 
    $img = 0;

$ocache_mem_info = null;
$ocache_file_info = null;
$ocache_summary_info = null;
$fcache_mem_info = null;
$fcache_file_info = null;
$fcache_summary_info = null;
$rpcache_mem_info = null;
$rpcache_file_info = null;

function convert_bytes_to_string( $bytes ) {
    $units = array( 0 => 'B', 1 => 'kB', 2 => 'MB', 3 => 'GB' );
    $log = log( $bytes, 1024 );
    $power = (int) $log;
    $size = pow(1024, $log - $power);
    return round($size, 2) . ' ' . $units[$power];
}

function seconds_to_words( $seconds ) {
    /*** return value ***/
    $ret = "";

    /*** get the hours ***/
    $hours = intval(intval( $seconds ) / 3600);
    if ( $hours > 0 ) {
        $ret .= "$hours hours ";
    }
    /*** get the minutes ***/
    $minutes = bcmod( ( intval( $seconds ) / 60 ), 60 );
    if( $hours > 0 || $minutes > 0 ) {
        $ret .= "$minutes minutes ";
    }

    /*** get the seconds ***/
    $seconds = bcmod( intval( $seconds ), 60 );
    $ret .= "$seconds seconds";

    return $ret;
}

function get_trimmed_filename( $filepath, $max_len ) {
    if ($max_len <= 0) die ('The maximum allowed length must be bigger than 0');
    
    $result = basename( $filepath );
    if ( strlen( $result ) > $max_len ) 
        $result = substr( $result, -1 * $max_len );
        
    return $result;
}

function get_trimmed_string( $input, $max_len ) {
    if ($max_len <= 3) die ('The maximum allowed length must be bigger than 3');
    
    $result = $input;
    if ( strlen( $result ) > $max_len ) 
        $result = substr( $result, 0, $max_len - 3 ). '...';
        
    return $result;	
}

function get_trimmed_ini_value( $input, $max_len, $separators = array('|', ',') ) {
    if ($max_len <= 3) die ('The maximum allowed length must be bigger than 3');
    
    $result = $input;
    $lastindex = 0;
    if ( strlen( $result ) > $max_len ) {
        $result = substr( $result, 0, $max_len - 3 ).'...';
        if ( !is_array( $separators ) ) die( 'The separators must be in an array' );
        foreach ( $separators as $separator ) {
            $index = strripos( $result, $separator );
            if ( $index !== false  && $index > $lastindex )
                $lastindex = $index;
        }
        if ( 0 < $lastindex && $lastindex < ( $max_len - 3 ) )
            $result = substr( $result, 0, $lastindex + 1 ).'...';
    }
    return $result;
}

function get_ocache_summary( $entries ) {
    $result = array();
    $result['total_classes'] = 0;
    $result['total_functions'] = 0;
    $result['oldest_entry'] = '';
    $result['recent_entry'] = '';

    if ( isset( $entries ) && count( $entries ) > 0 && isset( $entries[1]['file_name'] ) ) {
        $oldest_time = $entries[1]['add_time'];
        $recent_time = $oldest_time;
        $result['oldest_entry'] = $entries[1]['file_name'];
        $result['recent_entry'] = $result['oldest_entry'];
        
        foreach ( (array)$entries as $entry ) {
            $result['total_classes'] += $entry['class_count'];
            $result['total_functions'] += $entry['function_count'];
            if ( $entry['add_time'] > $oldest_time ) {
                $oldest_time = $entry['add_time'];
                $result['oldest_entry'] = $entry['file_name'];
            }
            if ( $entry['add_time'] < $recent_time ) {
                $recent_time = $entry['add_time'];
                $result['recent_entry'] = $entry['file_name'];
            }
        }
    }
    return $result;
}
function get_fcache_summary( $entries ) {
    $result = array();
    $result['total_size'] = 0;
    $result['oldest_entry'] = '';
    $result['recent_entry'] = '';

    if ( isset( $entries ) && count( $entries ) > 0 && isset( $entries[1]['file_name'] ) ) {
        $oldest_time = $entries[1]['add_time'];
        $recent_time = $oldest_time;
        $result['oldest_entry'] = $entries[1]['file_name'];
        $result['recent_entry'] = $result['oldest_entry'];
        
        foreach ( (array)$entries as $entry ) {
            $result['total_size'] += $entry['file_size'];
            if ( $entry['add_time'] > $oldest_time ) {
                $oldest_time = $entry['add_time'];
                $result['oldest_entry'] = $entry['file_name'];
            }
            if ( $entry['add_time'] < $recent_time ) {
                $recent_time = $entry['add_time'];
                $result['recent_entry'] = $entry['file_name'];
            }
        }
    }
    return $result;
}

function gd_loaded() {
    return extension_loaded( 'gd' );
}

if ( $img > 0 ) {
    if ( !gd_loaded() )
        exit( 0 );
    
    function create_hit_miss_chart( $width, $height, $hits, $misses, $title = 'Hits & Misses (in %)' ) {
        
        $hit_percent = 0;
        $miss_percent = 0;
        if ( $hits < 0 ) $hits = 0;
        if ( $misses < 0 ) $misses = 0;
        if ( $hits > 0 || $misses > 0 ) {
            $hit_percent = round( $hits / ( $hits + $misses ) * 100, 2 );
            $miss_percent = round( $misses / ( $hits + $misses ) * 100, 2 );
        }
        $data = array( 'Hits' => $hit_percent, 'Misses' => $miss_percent );		
        
        $image = imagecreate( $width, $height ); 

        // colors 
        $white = imagecolorallocate( $image, 0xFF, 0xFF, 0xFF );
        $phpblue = imagecolorallocate( $image, 0x5C, 0x87, 0xB2 ); 
        $black = imagecolorallocate( $image, 0x00, 0x00, 0x00 ); 
        $gray = imagecolorallocate( $image, 0xC0, 0xC0, 0xC0 );

        $maxval = max( $data );
        $nval = sizeof( $data );

        // draw something here
        $hmargin = 38; // left horizontal margin for y-labels 
        $vmargin = 20; // top (bottom) vertical margin for title (x-labels) 

        $base = floor( ( $width - $hmargin ) / $nval );

        $xsize = $nval * $base - 1; // x-size of plot
        $ysize = $height - 2 * $vmargin; // y-size of plot
    
        // plot frame 
        imagerectangle( $image, $hmargin, $vmargin, $hmargin + $xsize, $vmargin + $ysize, $black ); 

        // top label
        $titlefont = 3;
        $txtsize = imagefontwidth( $titlefont ) * strlen( $title );
        $xpos = (int)( $hmargin + ( $xsize - $txtsize ) / 2 );
        $xpos = max( 1, $xpos ); // force positive coordinates 
        $ypos = 3; // distance from top 
        imagestring( $image, $titlefont, $xpos, $ypos, $title , $black ); 

        // grid lines
        $labelfont = 2;
        $ngrid = 4;

        $dydat = 100 / $ngrid;
        $dypix = $ysize / $ngrid;

        for ( $i = 0; $i <= $ngrid; $i++ ) {
            $ydat = (int)( $i * $dydat );
            $ypos = $vmargin + $ysize - (int)( $i * $dypix );
        
            $txtsize = imagefontwidth( $labelfont ) * strlen( $ydat );
            $txtheight = imagefontheight( $labelfont );
        
            $xpos = (int)( ( $hmargin - $txtsize) / 2 );
            $xpos = max( 1, $xpos );
        
            imagestring( $image, $labelfont, $xpos, $ypos - (int)( $txtheight/2 ), $ydat, $black );
        
            if ( !( $i == 0 ) && !( $i >= $ngrid ) ) 
                imageline( $image, $hmargin - 3, $ypos, $hmargin + $xsize, $ypos, $gray ); 
                // don't draw at Y=0 and top 
        }

        // graph bars
        // columns and x labels 
        $padding = 30; // half of spacing between columns 
        $yscale = $ysize / ( $ngrid * $dydat ); // pixels per data unit 

        for ( $i = 0; list( $xval, $yval ) = each( $data ); $i++ ) { 

            // vertical columns 
            $ymax = $vmargin + $ysize; 
            $ymin = $ymax - (int)( $yval * $yscale ); 
            $xmax = $hmargin + ( $i + 1 ) * $base - $padding; 
            $xmin = $hmargin + $i * $base + $padding; 

            imagefilledrectangle( $image, $xmin, $ymin, $xmax, $ymax, $phpblue ); 

            // x labels 
            $xlabel = $xval.': '.$yval.'%';
            $txtsize = imagefontwidth( $labelfont ) * strlen( $xlabel );

            $xpos = ( $xmin + $xmax - $txtsize ) / 2;
            $xpos = max( $xmin, $xpos ); 
            $ypos = $ymax + 3; // distance from x axis

            imagestring( $image, $labelfont, $xpos, $ypos, $xlabel, $black ); 
        }
        return $image;
    }

    $image = null;
    switch( $img ) {
        case OCACHE_DATA: {
            $ocache_file_info = wincache_ocache_fileinfo();
            $image = create_hit_miss_chart(IMG_WIDTH, IMG_HEIGHT, $ocache_file_info['total_hit_count'], $ocache_file_info['total_miss_count'], 'Opcode Cache Hits & Misses (in %)');
            break;
        }
        case FCACHE_DATA: {
            $fcache_file_info = wincache_fcache_fileinfo();
            $image = create_hit_miss_chart(IMG_WIDTH, IMG_HEIGHT, $fcache_file_info['total_hit_count'], $fcache_file_info['total_miss_count'], 'File Cache Hits & Misses (in %)');
        }
    }

    if ( $image !== null ) {
        // flush image 
        header('Content-type: image/png'); // or "Content-type: image/png" 
        imagepng($image); // or imagepng($image) 
        imagedestroy($image); 
    }
    exit;
}

function get_chart_markup( $chart_type ) {
    global $PHP_SELF;
    $result = '';
    $alt_title = '';
            
    if ( gd_loaded() ){
        if ( $chart_type == OCACHE_DATA )
            $alt_title = 'Opcode cache hit and miss percentage chart';
        elseif ($chart_type == FCACHE_DATA )
            $alt_title = "File cache hit and miss percentage chart";
        else 
            return '';

        $result = '<img src="'.$PHP_SELF.'?img='.$chart_type.'" alt="'.$alt_title.'" width="'.IMG_WIDTH.'" height="'.IMG_HEIGHT.'" />';
    }
    else {
        $result = '<p class="notice">Enable GD library (<em>php_gd2.dll</em>) in order to see the charts.</p>';
    }

    return $result;
}

function init_cache_info( $cache_type = SUMMARY_DATA )
{
    global	$ocache_mem_info, 
            $ocache_file_info,
            $ocache_summary_info,
            $fcache_mem_info,
            $fcache_file_info,
            $fcache_summary_info,
            $rpcache_mem_info,
            $rpcache_file_info;

    if ( $cache_type == SUMMARY_DATA || $cache_type == OCACHE_DATA ) {
        $ocache_mem_info = wincache_ocache_meminfo();
        $ocache_file_info = wincache_ocache_fileinfo();
        $ocache_summary_info = get_ocache_summary( $ocache_file_info['file_entries'] );
    }
    if ( $cache_type == SUMMARY_DATA || $cache_type == FCACHE_DATA ) {
        $fcache_mem_info = wincache_fcache_meminfo();
        $fcache_file_info = wincache_fcache_fileinfo();
        $fcache_summary_info = get_fcache_summary( $fcache_file_info['file_entries'] );		
    }
    if ( $cache_type == SUMMARY_DATA || $cache_type == RCACHE_DATA ){
        $rpcache_mem_info = wincache_rplist_meminfo();
        $rpcache_file_info = wincache_rplist_fileinfo();
    }
}

?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">

<head>
<style type="text/css">
body {
    background-color: #ffffff;
    color: #000000;
    font-family: sans-serif;
    font-size: 0.8em;
}
h1 {
    font-size: 2em;
}
#content {
    width: 880px;
    margin: 1em;
}
#header {
    color: #ffffff;
    border: 1px solid black;
    background-color: #5C87B2;
    margin-bottom: 1em;
    padding: 1em 2em;
}
/*The #menu element credits: */
/*Credits: Dynamic Drive CSS Library */
/*URL: http://www.dynamicdrive.com/style/ */
#menu {
    width: 100%;
    overflow: hidden;
    border-bottom: 1px solid black;
/*bottom horizontal line that runs beneath tabs*/	margin-bottom: 1em;
}
#menu ul {
    margin: 0;
    padding: 0;
    padding-left: 10px; /*offset of tabs relative to browser left edge*/;
    font-weight: bold;
    font-size: 1.2em;
    list-style-type: none;
}
#menu li {
    display: inline;
    margin: 0;
}
#menu li a {
    float: left;
    display: block;
    text-decoration: none;
    margin: 0;
    padding: 7px 8px;
/*padding inside each tab*/	border-right: 1px solid white;
/*right divider between tabs*/	color: white;
    background: #5C87B2; /*background of tabs (default state)*/
}
#menu li a:visited {
    color: white;
}
#menu li a:hover, #menu li.selected a {
    background: #336699;
}
/*The end of the menu elements credits */
.overview{
    float: left;
    width: inherit;
    margin-bottom: 2em;
}
.list{
    float: left;
    width: 100%;
    margin-bottom: 2em;
}
.tabledata_left{
    float: left;
    width: 500px;
    margin-right: 20px;
}
.tabledata_right, .graphdata_right{
    float:left;
    width: 360px;
}
.extra_margin{
    margin-top: 20px;
}
table {
    border-collapse: collapse;
}
td, th {
    border: 1px solid black;
    vertical-align: baseline;
}
th {
    background-color: #5C87B2;
    font-weight: bold;
    color: #ffffff;
}
.e {
    background-color: #cbe1ef;
    font-weight: bold;
    color: #000000;
    width: 40%;
}
.v {
    background-color: #E7E7E7;
    color: #000000;
}
.notice {
    display: block;
    margin-top: 1.5em;
    padding: 1em;
    background-color: #ffffe0;
    border: 1px solid #dddddd;
}
.clear{
    clear: both;
}
</style>
<title>Windows Cache Extension for PHP - Statistics</title>
</head>

<body>

<div id="content">
    <div id="header">
        <h1>Windows Cache Extension for PHP - Statistics</h1>
    </div>
    <div id="menu">
        <ul>
            <li <?php echo ($page == SUMMARY_DATA)? 'class="selected"' : ''; ?>><a href="<?php echo $PHP_SELF, '?page=', SUMMARY_DATA; ?>">Summary</a></li>
            <li <?php echo ($page == OCACHE_DATA)? 'class="selected"' : ''; ?>><a href="<?php echo $PHP_SELF, '?page=', OCACHE_DATA; ?>">Opcode Cache</a></li>
            <li <?php echo ($page == FCACHE_DATA)? 'class="selected"' : ''; ?>><a href="<?php echo $PHP_SELF, '?page=', FCACHE_DATA; ?>">File System Cache</a></li>
            <li <?php echo ($page == RCACHE_DATA)? 'class="selected"' : ''; ?>><a href="<?php echo $PHP_SELF, '?page=', RCACHE_DATA; ?>">Relative Path Cache</a></li>
        </ul>
    </div>
<?php if ( $page == SUMMARY_DATA ) { 
    init_cache_info( SUMMARY_DATA );
?>
    <div class="overview">
        <div class="tabledata_left">
            <table style="width: 100%">
                <tr>
                    <th colspan="2">General Information</th>
                </tr>
                <tr>
                    <td class="e">WinCache version</td>
                    <td class="v"><?php echo phpversion('wincache'); ?></td>
                </tr>
                <tr>
                    <td class="e">PHP version</td>
                    <td class="v"><?php echo phpversion(); ?></td>
                </tr>
                <tr title="<?php echo $_SERVER['DOCUMENT_ROOT']; ?>">
                    <td class="e">Document root</td>
                    <td class="v"><?php echo get_trimmed_string( $_SERVER['DOCUMENT_ROOT'], PATH_MAX_LENGTH ); ?></td>
                </tr>
                <tr title="<?php echo isset( $_SERVER['PHPRC'] ) ? $_SERVER['PHPRC'] : 'Not defined'; ?>">
                    <td class="e">PHPRC</td>
                    <td class="v"><?php echo isset( $_SERVER['PHPRC'] ) ? get_trimmed_string( $_SERVER['PHPRC'], PATH_MAX_LENGTH ) : 'Not defined'; ?></td>
                </tr>
                <tr>
                    <td class="e">Server software</td>
                    <td class="v"><?php echo isset( $_SERVER['SERVER_SOFTWARE'] ) ? $_SERVER['SERVER_SOFTWARE']: 'Not set'; ?></td>
                </tr>
                <tr>
                    <td class="e">Operating System</td>
                    <td class="v"><?php echo php_uname( 's' ), ' ', php_uname( 'r' ), ' ', php_uname( 'v' ); ?></td>
                </tr>
                  <tr>
                    <td class="e">Processor information</td>
                    <td class="v"><?php echo isset( $_SERVER['PROCESSOR_IDENTIFIER'] ) ? $_SERVER['PROCESSOR_IDENTIFIER']: 'Not set'; ?></td>
                </tr>
                <tr>
                    <td class="e">Number of processors</td>
                    <td class="v"><?php echo isset( $_SERVER['NUMBER_OF_PROCESSORS'] ) ? $_SERVER['NUMBER_OF_PROCESSORS']: 'Not set'; ?></td>
                </tr>
                <tr>
                    <td class="e">Machine name</td>
                    <td class="v"><?php echo (getenv( 'COMPUTERNAME' ) != FALSE) ? getenv( 'COMPUTERNAME' ) : 'Not set'; ?></td>
                </tr>
                <tr>
                    <td class="e">Host name</td>
                    <td class="v"><?php echo isset( $_SERVER['HTTP_HOST'] ) ? $_SERVER['HTTP_HOST'] : 'Not set'; ?></td>
                </tr>
                <tr>
                    <td class="e">Available memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $ocache_mem_info['memory_total'] );?></td>
                </tr>
                <tr>
                    <td class="e">Cache uptime</td>
                    <td class="v"><?php echo ( isset( $ocache_file_info['total_cache_uptime'] ) ) ? seconds_to_words( $ocache_file_info['total_cache_uptime'] ) : "Unknown"; ?></td>
                </tr>
            </table>
        </div>
        <div class="tabledata_right">
            <table style="width:100%">
                <tr>
                    <th colspan="2">Cache Settings</th>
                </tr>
<?php 
foreach ( ini_get_all( 'wincache' ) as $ini_name => $ini_value) {
    // Do not show the settings used for debugging
    if ( in_array( $ini_name, $settings_to_hide ) ) 
        continue;
    echo '<tr title="', $ini_value['local_value'], '"><td class="e">', $ini_name, '</td><td class="v">';
    if ( !is_numeric( $ini_value['local_value'] ) )
        echo get_trimmed_ini_value( $ini_value['local_value'], INI_MAX_LENGTH );
    else
        echo $ini_value['local_value'];
    echo '</td></tr>', "\n";
} 
?>				
            </table>
        </div>
    </div>
    <div class="overview">
        <div class="tabledata_left extra_margin">
            <table width="100%">
                <tr>
                    <th colspan="2">Opcode Cache Overview</th>
                </tr>
                <tr>
                    <td class="e">Cached files</td>
                    <td class="v"><a href="<?php echo $PHP_SELF, '?page=2#filelist'; ?>"><?php echo $ocache_file_info['total_file_count']; ?></a></td>
                </tr>
                <tr>
                    <td class="e">Hits</td>
                    <td class="v"><?php echo $ocache_file_info['total_hit_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Misses</td>
                    <td class="v"><?php echo $ocache_file_info['total_miss_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Available memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $ocache_mem_info['memory_free'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Memory overhead</td>
                    <td class="v"><?php echo convert_bytes_to_string( $ocache_mem_info['memory_overhead'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Number of functions</td>
                    <td class="v"><?php echo $ocache_summary_info['total_functions']; ?></td>
                </tr>
                <tr>
                    <td class="e">Number of classes</td>
                    <td class="v"><?php echo $ocache_summary_info['total_classes']; ?></td>
                </tr>
                <tr title="<?php echo $ocache_summary_info['oldest_entry']; ?>">
                    <td class="e">Oldest entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $ocache_summary_info['oldest_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
                <tr title="<?php echo $ocache_summary_info['recent_entry']; ?>">
                    <td class="e">Most recent entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $ocache_summary_info['recent_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
            </table>
        </div>
        <div class="graphdata_right">
            <?php echo get_chart_markup( OCACHE_DATA ); ?>
        </div>
    </div>
    <div class="overview">
        <div class="tabledata_left extra_margin">
            <table style="width: 100%">
                <tr>
                    <th colspan="2">File Cache Overview</th>
                </tr>
                <tr>
                    <td class="e">Cached files</td>
                    <td class="v"><a href="<?php echo $PHP_SELF, '?page=3#filelist'; ?>"><?php echo $fcache_file_info['total_file_count']; ?></a></td>
                </tr>
                <tr>
                    <td class="e">Total size of cached files</td>
                    <td class="v"><?php echo convert_bytes_to_string( $fcache_summary_info['total_size'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Hits</td>
                    <td class="v"><?php echo $fcache_file_info['total_hit_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Misses</td>
                    <td class="v"><?php echo $fcache_file_info['total_miss_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Available memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $fcache_mem_info['memory_free'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Memory overhead</td>
                    <td class="v"><?php echo convert_bytes_to_string( $fcache_mem_info['memory_overhead'] ); ?></td>
                </tr>
                <tr title="<?php echo $fcache_summary_info['oldest_entry']; ?>">
                    <td class="e">Oldest entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $fcache_summary_info['oldest_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
                <tr title="<?php echo $fcache_summary_info['recent_entry']; ?>">
                    <td class="e">Most recent entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $fcache_summary_info['recent_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
            </table>
        </div>
        <div class="graphdata_right">
            <?php echo get_chart_markup( FCACHE_DATA ); ?>
        </div>
    </div>
    <div class="overview">
        <div class="tabledata_left">
            <table style="width: 100%">
                <tr>
                    <th colspan="2">Relative Path Cache Overview</th>
                </tr>
                <tr>
                    <td class="e">Cached entries</td>
                    <td class="v"><a href="<?php echo $PHP_SELF, '?page=4#filelist'; ?>"><?php echo $rpcache_file_info['total_file_count'] ?></a></td>
                </tr>
                <tr>
                    <td class="e">Total memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $rpcache_mem_info['memory_total'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Available memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $rpcache_mem_info['memory_free'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Memory overhead</td>
                    <td class="v"><?php echo convert_bytes_to_string( $rpcache_mem_info['memory_overhead'] ); ?></td>
                </tr>
            </table>
        </div>
    </div>
<?php } else if ( $page == OCACHE_DATA )  {
    init_cache_info( OCACHE_DATA );
?>
    <div class="overview">
        <div class="tabledata_left">
            <table width="100%">
                <tr>
                    <th colspan="2">Opcode Cache Overview</th>
                </tr>
                <tr>
                    <td class="e">Cached files</td>
                    <td class="v"><?php echo $ocache_file_info['total_file_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Hits</td>
                    <td class="v"><?php echo $ocache_file_info['total_hit_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Misses</td>
                    <td class="v"><?php echo $ocache_file_info['total_miss_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Available memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $ocache_mem_info['memory_free'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Memory overhead</td>
                    <td class="v"><?php echo convert_bytes_to_string( $ocache_mem_info['memory_overhead'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Number of functions</td>
                    <td class="v"><?php echo $ocache_summary_info['total_functions']; ?></td>
                </tr>
                <tr>
                    <td class="e">Number of classes</td>
                    <td class="v"><?php echo $ocache_summary_info['total_classes']; ?></td>
                </tr>
                <tr title="<?php echo $ocache_summary_info['oldest_entry']; ?>">
                    <td class="e">Oldest entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $ocache_summary_info['oldest_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
                <tr title="<?php echo $ocache_summary_info['recent_entry']; ?>">
                    <td class="e">Most recent entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $ocache_summary_info['recent_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
            </table>
        </div>
        <div class="graphdata_right">
            <?php echo get_chart_markup( OCACHE_DATA ); ?>
        </div>
    </div>
    <div class="list" id="filelist">
        <table style="width:100%">
            <tr>
                <th colspan="7">Opcode cache entries</th>
            </tr>
            <tr>
                <th>File name</th>
                <th>Function count</th>
                <th>Class count</th>
                <th>Add time</th>
                <th>Use time</th>
                <th>Last check</th>
                <th>Hit count</th>
            </tr>
<?php 
    foreach ( $ocache_file_info['file_entries'] as $entry ) {
        echo '<tr title="', $entry['file_name'] ,'">', "\n";
        echo '<td class="e">', get_trimmed_filename( $entry['file_name'], PATH_MAX_LENGTH ),'</td>', "\n";
        echo '<td class="v">', $entry['function_count'],'</td>', "\n";
        echo '<td class="v">', $entry['class_count'],'</td>', "\n";
        echo '<td class="v">', $entry['add_time'],'</td>', "\n";
        echo '<td class="v">', $entry['use_time'],'</td>', "\n";
        echo '<td class="v">', $entry['last_check'],'</td>', "\n";
        echo '<td class="v">', $entry['hit_count'],'</td>', "\n";
        echo "</tr>\n";
    }
?>
        </table>
    </div>
<?php } else if ( $page == FCACHE_DATA ) {
    init_cache_info( FCACHE_DATA );
?>
    <div class="overview">
        <div class="tabledata_left">
            <table style="width: 100%">
                <tr>
                    <th colspan="2">File Cache Overview</th>
                </tr>
                <tr>
                    <td class="e">Cached files</td>
                    <td class="v"><?php echo $fcache_file_info['total_file_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Total size of cached files</td>
                    <td class="v"><?php echo convert_bytes_to_string( $fcache_summary_info['total_size'] ); ?></td>
                </tr>                    
                <tr>
                    <td class="e">Hits</td>
                    <td class="v"><?php echo $fcache_file_info['total_hit_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Misses</td>
                    <td class="v"><?php echo $fcache_file_info['total_miss_count']; ?></td>
                </tr>
                <tr>
                    <td class="e">Available memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $fcache_mem_info['memory_free'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Memory overhead</td>
                    <td class="v"><?php echo convert_bytes_to_string( $fcache_mem_info['memory_overhead'] ); ?></td>
                </tr>
                <tr title="<?php echo $fcache_summary_info['oldest_entry']; ?>">
                    <td class="e">Oldest entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $fcache_summary_info['oldest_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
                <tr title="<?php echo $fcache_summary_info['recent_entry']; ?>">
                    <td class="e">Most recent entry</td>
                    <td class="v"><?php echo get_trimmed_filename( $fcache_summary_info['recent_entry'], PATH_MAX_LENGTH ); ?></td>
                </tr>
            </table>
        </div>
        <div class="graphdata_right">
            <?php echo get_chart_markup( FCACHE_DATA ); ?>
        </div>
    </div>
    <div class="list" id="filelist">
        <table style="width:100%">
            <tr>
                <th colspan="6">Opcode cache entries</th>
            </tr>
            <tr>
                <th>File name</th>
                <th>Add time</th>
                <th>Use time</th>
                <th>Last check</th>
                <th>File size</th>
                <th>Hit Count</th>
        </tr>
<?php 
    foreach ( $fcache_file_info['file_entries'] as $entry ) {
        echo '<tr title="', $entry['file_name'] ,'">', "\n";
        echo '<td class="e">', get_trimmed_filename( $entry['file_name'], PATH_MAX_LENGTH ),'</td>', "\n";
        echo '<td class="v">', $entry['add_time'],'</td>', "\n";
        echo '<td class="v">', $entry['use_time'],'</td>', "\n";
        echo '<td class="v">', $entry['last_check'],'</td>', "\n";
        echo '<td class="v">', convert_bytes_to_string( $entry['file_size'] ),'</td>', "\n";
        echo '<td class="v">', $entry['hit_count'],'</td>', "\n";
        echo "</tr>\n";
    }
?>
        </table>
    </div>

<?php } else if ( $page == 4 ) {
    init_cache_info( RCACHE_DATA );
?>
    <div class="overview">
        <div class="tabledata_left">
            <table style="width: 100%">
                <tr>
                    <th colspan="2">Relative Path Cache Overview</th>
                </tr>
                <tr>
                    <td class="e">Cached entries</td>
                    <td class="v"><?php echo $rpcache_file_info['total_file_count'] ?></td>
                </tr>
                <tr>
                    <td class="e">Total memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $rpcache_mem_info['memory_total'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Available memory</td>
                    <td class="v"><?php echo convert_bytes_to_string( $rpcache_mem_info['memory_free'] ); ?></td>
                </tr>
                <tr>
                    <td class="e">Memory overhead</td>
                    <td class="v"><?php echo convert_bytes_to_string( $rpcache_mem_info['memory_overhead'] ); ?></td>
                </tr>
            </table>
        </div>
    </div>
    <div class="list" id="filelist">
        <table style="width:100%">
            <tr>
                <th colspan="2">Relative path cache entries</th>
            </tr>
            <tr>
                <th>Relative path</th>
                <th>Subkey data</th>
        </tr>
<?php 
    foreach ( $rpcache_file_info['rplist_entries'] as $entry ) {
        echo '<tr title="',$entry['subkey_data'], '">', "\n";
        echo '<td class="e">', get_trimmed_string( $entry['relative_path'], PATH_MAX_LENGTH ),'</td>', "\n";
        echo '<td class="v">', get_trimmed_string( $entry['subkey_data'], SUBKEY_MAX_LENGTH ), '</td>', "\n";
        echo "</tr>\n";
    }
?>
        </table>
    </div>
<?php } ?>
<div class="clear"></div>
</div>
</body>

</html>
