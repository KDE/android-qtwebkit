<?php

require_once(dirname(__FILE__).'/functions-compat.php');

if (!function_exists('_')) {
	function _($string) {
		return $string;
	}
}

function get_profile($field, $user = false) {
	global $wpdb;
	if (!$user)
		$user = $wpdb->escape($_COOKIE['wordpressuser_' . COOKIEHASH]);
	return $wpdb->get_var("SELECT $field FROM $wpdb->users WHERE user_login = '$user'");
}

function mysql2date($dateformatstring, $mysqlstring, $translate = true) {
	global $month, $weekday, $month_abbrev, $weekday_abbrev;
	$m = $mysqlstring;
	if (empty($m)) {
		return false;
	}
	$i = mktime(substr($m,11,2),substr($m,14,2),substr($m,17,2),substr($m,5,2),substr($m,8,2),substr($m,0,4)); 
	if (!empty($month) && !empty($weekday) && $translate) {
		$datemonth = $month[date('m', $i)];
		$datemonth_abbrev = $month_abbrev[$datemonth];
		$dateweekday = $weekday[date('w', $i)];
		$dateweekday_abbrev = $weekday_abbrev[$dateweekday]; 		
		$dateformatstring = ' '.$dateformatstring;
		$dateformatstring = preg_replace("/([^\\\])D/", "\\1".backslashit($dateweekday_abbrev), $dateformatstring);
		$dateformatstring = preg_replace("/([^\\\])F/", "\\1".backslashit($datemonth), $dateformatstring);
		$dateformatstring = preg_replace("/([^\\\])l/", "\\1".backslashit($dateweekday), $dateformatstring);
		$dateformatstring = preg_replace("/([^\\\])M/", "\\1".backslashit($datemonth_abbrev), $dateformatstring);
	
		$dateformatstring = substr($dateformatstring, 1, strlen($dateformatstring)-1);
	}
	$j = @date($dateformatstring, $i);
	if (!$j) {
	// for debug purposes
	//	echo $i." ".$mysqlstring;
	}
	return $j;
}

function current_time($type, $gmt = 0) {
	switch ($type) {
		case 'mysql':
			if ($gmt) $d = gmdate('Y-m-d H:i:s');
			else $d = gmdate('Y-m-d H:i:s', (time() + (get_settings('gmt_offset') * 3600)));
			return $d;
			break;
		case 'timestamp':
			if ($gmt) $d = time();
			else $d = time() + (get_settings('gmt_offset') * 3600);
			return $d;
			break;
	}
}

function date_i18n($dateformatstring, $unixtimestamp) {
	global $month, $weekday, $month_abbrev, $weekday_abbrev;
	$i = $unixtimestamp; 
	if ((!empty($month)) && (!empty($weekday))) {
		$datemonth = $month[date('m', $i)];
		$datemonth_abbrev = $month_abbrev[$datemonth];
		$dateweekday = $weekday[date('w', $i)];
		$dateweekday_abbrev = $weekday_abbrev[$dateweekday]; 		
		$dateformatstring = ' '.$dateformatstring;
		$dateformatstring = preg_replace("/([^\\\])D/", "\\1".backslashit($dateweekday_abbrev), $dateformatstring);
		$dateformatstring = preg_replace("/([^\\\])F/", "\\1".backslashit($datemonth), $dateformatstring);
		$dateformatstring = preg_replace("/([^\\\])l/", "\\1".backslashit($dateweekday), $dateformatstring);
		$dateformatstring = preg_replace("/([^\\\])M/", "\\1".backslashit($datemonth_abbrev), $dateformatstring);
		$dateformatstring = substr($dateformatstring, 1, strlen($dateformatstring)-1);
	}
	$j = @date($dateformatstring, $i);
	return $j;
	}

function get_weekstartend($mysqlstring, $start_of_week) {
	$my = substr($mysqlstring,0,4);
	$mm = substr($mysqlstring,8,2);
	$md = substr($mysqlstring,5,2);
	$day = mktime(0,0,0, $md, $mm, $my);
	$weekday = date('w',$day);
	$i = 86400;

	if ($weekday < get_settings('start_of_week'))
		$weekday = 7 - (get_settings('start_of_week') - $weekday);

	while ($weekday > get_settings('start_of_week')) {
		$weekday = date('w',$day);
		if ($weekday < get_settings('start_of_week'))
			$weekday = 7 - (get_settings('start_of_week') - $weekday);

		$day = $day - 86400;
		$i = 0;
	}
	$week['start'] = $day + 86400 - $i;
	//$week['end']   = $day - $i + 691199;
	$week['end'] = $week['start'] + 604799;
	return $week;
}

function get_lastpostdate($timezone = 'server') {
	global $cache_lastpostdate, $pagenow, $wpdb;
	$add_seconds_blog = get_settings('gmt_offset') * 3600;
	$add_seconds_server = date('Z');
	$now = current_time('mysql', 1);
	if ( !isset($cache_lastpostdate[$timezone]) ) {
		switch(strtolower($timezone)) {
			case 'gmt':
				$lastpostdate = $wpdb->get_var("SELECT post_date_gmt FROM $wpdb->posts WHERE post_date_gmt <= '$now' AND post_status = 'publish' ORDER BY post_date_gmt DESC LIMIT 1");
				break;
			case 'blog':
				$lastpostdate = $wpdb->get_var("SELECT post_date FROM $wpdb->posts WHERE post_date_gmt <= '$now' AND post_status = 'publish' ORDER BY post_date_gmt DESC LIMIT 1");
				break;
			case 'server':
				$lastpostdate = $wpdb->get_var("SELECT DATE_ADD(post_date_gmt, INTERVAL '$add_seconds_server' SECOND) FROM $wpdb->posts WHERE post_date_gmt <= '$now' AND post_status = 'publish' ORDER BY post_date_gmt DESC LIMIT 1");
				break;
		}
		$cache_lastpostdate[$timezone] = $lastpostdate;
	} else {
		$lastpostdate = $cache_lastpostdate[$timezone];
	}
	return $lastpostdate;
}

function get_lastpostmodified($timezone = 'server') {
	global $cache_lastpostmodified, $pagenow, $wpdb;
	$add_seconds_blog = get_settings('gmt_offset') * 3600;
	$add_seconds_server = date('Z');
	$now = current_time('mysql', 1);
	if ( !isset($cache_lastpostmodified[$timezone]) ) {
		switch(strtolower($timezone)) {
			case 'gmt':
				$lastpostmodified = $wpdb->get_var("SELECT post_modified_gmt FROM $wpdb->posts WHERE post_modified_gmt <= '$now' AND post_status = 'publish' ORDER BY post_modified_gmt DESC LIMIT 1");
				break;
			case 'blog':
				$lastpostmodified = $wpdb->get_var("SELECT post_modified FROM $wpdb->posts WHERE post_modified_gmt <= '$now' AND post_status = 'publish' ORDER BY post_modified_gmt DESC LIMIT 1");
				break;
			case 'server':
				$lastpostmodified = $wpdb->get_var("SELECT DATE_ADD(post_modified_gmt, INTERVAL '$add_seconds_server' SECOND) FROM $wpdb->posts WHERE post_modified_gmt <= '$now' AND post_status = 'publish' ORDER BY post_modified_gmt DESC LIMIT 1");
				break;
		}
		$lastpostdate = get_lastpostdate($timezone);
		if ($lastpostdate > $lastpostmodified) {
			$lastpostmodified = $lastpostdate;
		}
		$cache_lastpostmodified[$timezone] = $lastpostmodified;
	} else {
		$lastpostmodified = $cache_lastpostmodified[$timezone];
	}
	return $lastpostmodified;
}

function user_pass_ok($user_login,$user_pass) {
	global $cache_userdata;
	if ( empty($cache_userdata[$user_login]) ) {
		$userdata = get_userdatabylogin($user_login);
	} else {
		$userdata = $cache_userdata[$user_login];
	}
	return (md5($user_pass) == $userdata->user_pass);
}


function get_usernumposts($userid) {
	global $wpdb;
	return $wpdb->get_var("SELECT COUNT(*) FROM $wpdb->posts WHERE post_author = '$userid' AND post_status = 'publish'");
}

// examine a url (supposedly from this blog) and try to
// determine the post ID it represents.
function url_to_postid($url) {
	global $wp_rewrite;

	// First, check to see if there is a 'p=N' or 'page_id=N' to match against:
	preg_match('#[?&](p|page_id)=(\d+)#', $url, $values);
	$id = intval($values[2]);
	if ($id) return $id;

	// URI is probably a permalink.
	$rewrite = $wp_rewrite->wp_rewrite_rules();

	if ( empty($rewrite) )
		return 0;

	$req_uri = $url;

	if ( false !== strpos($req_uri, get_settings('home')) ) {
		$req_uri = str_replace(get_settings('home'), '', $req_uri);
	} else {
		$home_path = parse_url(get_settings('home'));
		$home_path = $home_path['path'];
		$req_uri = str_replace($home_path, '', $req_uri);
	}

	$req_uri = trim($req_uri, '/');
	$request = $req_uri;
	
	// Look for matches.
	$request_match = $request;
	foreach ($rewrite as $match => $query) {
		// If the requesting file is the anchor of the match, prepend it
		// to the path info.
		if ((! empty($req_uri)) && (strpos($match, $req_uri) === 0)) {
			$request_match = $req_uri . '/' . $request;
		}

		if (preg_match("!^$match!", $request_match, $matches)) {
			// Got a match.
			// Trim the query of everything up to the '?'.
			$query = preg_replace("!^.+\?!", '', $query);
			
			// Substitute the substring matches into the query.
			eval("\$query = \"$query\";");
			$query = new WP_Query($query);
			if ( !empty($query->post) )
				return $query->post->ID;
			else
				return 0;
		}
	}

	return 0;
}


/* Options functions */

function get_settings($setting) {
  global $wpdb, $cache_settings, $cache_nonexistantoptions;
	if ( strstr($_SERVER['REQUEST_URI'], 'wp-admin/install.php') || defined('WP_INSTALLING') )
		return false;

	if ( empty($cache_settings) )
		$cache_settings = get_alloptions();

	if ( empty($cache_nonexistantoptions) )
		$cache_nonexistantoptions = array();

	if ('home' == $setting && '' == $cache_settings->home)
		return apply_filters('option_' . $setting, $cache_settings->siteurl);

	if ( isset($cache_settings->$setting) ) :
		return apply_filters('option_' . $setting, $cache_settings->$setting);
	else :
		// for these cases when we're asking for an unknown option
		if ( isset($cache_nonexistantoptions[$setting]) )
			return false;

		$option = $wpdb->get_var("SELECT option_value FROM $wpdb->options WHERE option_name = '$setting'");

		if (!$option) :
			$cache_nonexistantoptions[$setting] = true;
			return false;
		endif;

		@ $kellogs = unserialize($option);
		if ($kellogs !== FALSE)
			return apply_filters('option_' . $setting, $kellogs);
		else return apply_filters('option_' . $setting, $option);
	endif;
}

function get_option($option) {
	return get_settings($option);
}

function form_option($option) {
	echo htmlspecialchars( get_option($option), ENT_QUOTES );
}

function get_alloptions() {
	global $wpdb, $wp_queries;
	$wpdb->hide_errors();
	if (!$options = $wpdb->get_results("SELECT option_name, option_value FROM $wpdb->options WHERE autoload = 'yes'")) {
		$options = $wpdb->get_results("SELECT option_name, option_value FROM $wpdb->options");
	}
	$wpdb->show_errors();

	foreach ($options as $option) {
		// "When trying to design a foolproof system, 
		//  never underestimate the ingenuity of the fools :)" -- Dougal
		if ('siteurl' == $option->option_name) $option->option_value = preg_replace('|/+$|', '', $option->option_value);
		if ('home' == $option->option_name) $option->option_value = preg_replace('|/+$|', '', $option->option_value);
		if ('category_base' == $option->option_name) $option->option_value = preg_replace('|/+$|', '', $option->option_value);
		@ $value = unserialize($option->option_value);
		if ($value === FALSE)
			$value = $option->option_value;
		$all_options->{$option->option_name} = apply_filters('pre_option_' . $option->option_name, $value);
	}
	return apply_filters('all_options', $all_options);
}

function update_option($option_name, $newvalue) {
	global $wpdb, $cache_settings;
	if ( is_array($newvalue) || is_object($newvalue) )
		$newvalue = serialize($newvalue);

	$newvalue = trim($newvalue); // I can't think of any situation we wouldn't want to trim

    // If the new and old values are the same, no need to update.
    if ($newvalue == get_option($option_name)) {
        return true;
    }

	// If it's not there add it
	if ( !$wpdb->get_var("SELECT option_name FROM $wpdb->options WHERE option_name = '$option_name'") )
		add_option($option_name);

	$newvalue = $wpdb->escape($newvalue);
	$wpdb->query("UPDATE $wpdb->options SET option_value = '$newvalue' WHERE option_name = '$option_name'");
	$cache_settings = get_alloptions(); // Re cache settings
	return true;
}


// thx Alex Stapleton, http://alex.vort-x.net/blog/
function add_option($name, $value = '', $description = '', $autoload = 'yes') {
	global $wpdb;
	$original = $value;
	if ( is_array($value) || is_object($value) )
		$value = serialize($value);

	if( !$wpdb->get_var("SELECT option_name FROM $wpdb->options WHERE option_name = '$name'") ) {
		$name = $wpdb->escape($name);
		$value = $wpdb->escape($value);
		$description = $wpdb->escape($description);
		$wpdb->query("INSERT INTO $wpdb->options (option_name, option_value, option_description, autoload) VALUES ('$name', '$value', '$description', '$autoload')");

		if($wpdb->insert_id) {
			global $cache_settings;
			$cache_settings->{$name} = $original;
		}
	}
	return;
}

function delete_option($name) {
	global $wpdb;
	// Get the ID, if no ID then return
	$option_id = $wpdb->get_var("SELECT option_id FROM $wpdb->options WHERE option_name = '$name'");
	if (!$option_id) return false;
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = '$name'");
	return true;
}

function add_post_meta($post_id, $key, $value, $unique = false) {
	global $wpdb;
	
	if ($unique) {
		if( $wpdb->get_var("SELECT meta_key FROM $wpdb->postmeta WHERE meta_key
= '$key' AND post_id = '$post_id'") ) {
			return false;
		}
	}

	$wpdb->query("INSERT INTO $wpdb->postmeta
                                (post_id,meta_key,meta_value) 
                                VALUES ('$post_id','$key','$value')
                        ");
	
	return true;
}

function delete_post_meta($post_id, $key, $value = '') {
	global $wpdb;

	if (empty($value)) {
		$meta_id = $wpdb->get_var("SELECT meta_id FROM $wpdb->postmeta WHERE
post_id = '$post_id' AND meta_key = '$key'");
	} else {
		$meta_id = $wpdb->get_var("SELECT meta_id FROM $wpdb->postmeta WHERE
post_id = '$post_id' AND meta_key = '$key' AND meta_value = '$value'");
	}

	if (!$meta_id) return false;

	if (empty($value)) {
		$wpdb->query("DELETE FROM $wpdb->postmeta WHERE post_id = '$post_id'
AND meta_key = '$key'");
	} else {
		$wpdb->query("DELETE FROM $wpdb->postmeta WHERE post_id = '$post_id'
AND meta_key = '$key' AND meta_value = '$value'");
	}
        
	return true;
}

function get_post_meta($post_id, $key, $single = false) {
	global $wpdb, $post_meta_cache;

	if (isset($post_meta_cache[$post_id][$key])) {
		if ($single) {
			return $post_meta_cache[$post_id][$key][0];
		} else {
			return $post_meta_cache[$post_id][$key];
		}
	}

	$metalist = $wpdb->get_results("SELECT meta_value FROM $wpdb->postmeta WHERE post_id = '$post_id' AND meta_key = '$key'", ARRAY_N);

	$values = array();
	if ($metalist) {
		foreach ($metalist as $metarow) {
			$values[] = $metarow[0];
		}
	}

	if ($single) {
		if (count($values)) {
			return $values[0];
		} else {
			return '';
		}
	} else {
		return $values;
	}
}

function update_post_meta($post_id, $key, $value, $prev_value = '') {
	global $wpdb, $post_meta_cache;

		if(! $wpdb->get_var("SELECT meta_key FROM $wpdb->postmeta WHERE meta_key
= '$key' AND post_id = '$post_id'") ) {
			return false;
		}

	if (empty($prev_value)) {
		$wpdb->query("UPDATE $wpdb->postmeta SET meta_value = '$value' WHERE
meta_key = '$key' AND post_id = '$post_id'");
	} else {
		$wpdb->query("UPDATE $wpdb->postmeta SET meta_value = '$value' WHERE
meta_key = '$key' AND post_id = '$post_id' AND meta_value = '$prev_value'");
	}

	return true;
}

// Deprecated.  Use get_post().
function get_postdata($postid) {
	$post = &get_post($postid);
	
	$postdata = array (
		'ID' => $post->ID, 
		'Author_ID' => $post->post_author, 
		'Date' => $post->post_date, 
		'Content' => $post->post_content, 
		'Excerpt' => $post->post_excerpt, 
		'Title' => $post->post_title, 
		'Category' => $post->post_category,
		'post_status' => $post->post_status,
		'comment_status' => $post->comment_status,
		'ping_status' => $post->ping_status,
		'post_password' => $post->post_password,
		'to_ping' => $post->to_ping,
		'pinged' => $post->pinged,
		'post_name' => $post->post_name
	);

	return $postdata;
}

// Retrieves post data given a post ID or post object. 
// Handles post caching.
function &get_post(&$post, $output = OBJECT) {
	global $post_cache, $wpdb;

	if ( empty($post) ) {
		if ( isset($GLOBALS['post']) )
			$post = & $GLOBALS['post'];
		else
			$post = null;
	} elseif (is_object($post) ) {
		if (! isset($post_cache[$post->ID]))
			$post_cache[$post->ID] = &$post;
		$post = & $post_cache[$post->ID];
	} else {
		if (isset($post_cache[$post]))
			$post = & $post_cache[$post];
		else {
			$query = "SELECT * FROM $wpdb->posts WHERE ID=$post";
			$post_cache[$post] = & $wpdb->get_row($query);
			$post = & $post_cache[$post];
		}
	}

	if ( $output == OBJECT ) {
		return $post;
	} elseif ( $output == ARRAY_A ) {
		return get_object_vars($post);
	} elseif ( $output == ARRAY_N ) {
		return array_values(get_object_vars($post));
	} else {
		return $post;
	}
}

// Retrieves page data given a page ID or page object. 
// Handles page caching.
function &get_page(&$page, $output = OBJECT) {
	global $page_cache, $wpdb;

	if ( empty($page) ) {
		if ( isset($GLOBALS['page']) )
			$page = & $GLOBALS['page'];
		else
			$page = null;
	} elseif (is_object($page) ) {
		if (! isset($page_cache[$page->ID]))
			$page_cache[$page->ID] = &$page;
		$page = & $page_cache[$page->ID];
	} else {
		if ( isset($GLOBALS['page']) && ($page == $GLOBALS['page']->ID) )
			$page = & $GLOBALS['page'];
		elseif (isset($page_cache[$page]))
			$page = & $page_cache[$page];
		else {
			$query = "SELECT * FROM $wpdb->posts WHERE ID=$page";
			$page_cache[$page] = & $wpdb->get_row($query);
			$page = & $page_cache[$page];
		}
	}

	if ( $output == OBJECT ) {
		return $page;
	} elseif ( $output == ARRAY_A ) {
		return get_object_vars($page);
	} elseif ( $output == ARRAY_N ) {
		return array_values(get_object_vars($page));
	} else {
		return $page;
	}
}

// Retrieves category data given a category ID or category object. 
// Handles category caching.
function &get_category(&$category, $output = OBJECT) {
	global $cache_categories, $wpdb;

	if ( empty($category) )
		return null;

	$category = (int) $category;

	if ( ! isset($cache_categories))
		update_category_cache();

	if (is_object($category)) {
		if ( ! isset($cache_categories[$category->cat_ID]))
			$cache_categories[$category->cat_ID] = &$category;
		$category = & $cache_categories[$category->cat_ID];
	} else {
		if ( !isset($cache_categories[$category]) ) {
			$category = $wpdb->get_row("SELECT * FROM $wpdb->categories WHERE cat_ID = $category");
			$cache_categories[$category->cat_ID] = & $category;
		} else {
			$category = & $cache_categories[$category];
		}
	}

	if ( $output == OBJECT ) {
		return $category;
	} elseif ( $output == ARRAY_A ) {
		return get_object_vars($category);
	} elseif ( $output == ARRAY_N ) {
		return array_values(get_object_vars($category));
	} else {
		return $category;
	}
}

function get_catname($cat_ID) {
	$category = &get_category($cat_ID);
	return $category->cat_name;
}

function gzip_compression() {
	if ( strstr($_SERVER['PHP_SELF'], 'wp-admin') ) return false;
	if ( !get_settings('gzipcompression') ) return false;

	if( extension_loaded('zlib') ) {
		ob_start('ob_gzhandler');
	}
}


// functions to count the page generation time (from phpBB2)
// ( or just any time between timer_start() and timer_stop() )

function timer_stop($display = 0, $precision = 3) { //if called like timer_stop(1), will echo $timetotal
	global $timestart, $timeend;
	$mtime = microtime();
	$mtime = explode(' ',$mtime);
	$mtime = $mtime[1] + $mtime[0];
	$timeend = $mtime;
	$timetotal = $timeend-$timestart;
	if ($display)
		echo number_format($timetotal,$precision);
	return $timetotal;
}

function weblog_ping($server = '', $path = '') {
	global $wp_version;
	include_once (ABSPATH . WPINC . '/class-IXR.php');

	// using a timeout of 3 seconds should be enough to cover slow servers
	$client = new IXR_Client($server, ((!strlen(trim($path)) || ('/' == $path)) ? false : $path));
	$client->timeout = 3;
	$client->useragent .= ' -- WordPress/'.$wp_version;

	// when set to true, this outputs debug messages by itself
	$client->debug = false;
	$home = trailingslashit( get_option('home') );
	if ( !$client->query('weblogUpdates.extendedPing', get_settings('blogname'), $home, get_bloginfo('rss2_url') ) ) // then try a normal ping
		$client->query('weblogUpdates.ping', get_settings('blogname'), $home);
}

function generic_ping($post_id = 0) {
	$services = get_settings('ping_sites');
	$services = preg_replace("|(\s)+|", '$1', $services); // Kill dupe lines
	$services = trim($services);
	if ('' != $services) {
		$services = explode("\n", $services);
		foreach ($services as $service) {
			weblog_ping($service);
		}
	}

	return $post_id;
}

// Send a Trackback
function trackback($trackback_url, $title, $excerpt, $ID) {
	global $wpdb, $wp_version;

	if (empty($trackback_url))
		return;

	$title = urlencode($title);
	$excerpt = urlencode($excerpt);
	$blog_name = urlencode(get_settings('blogname'));
	$tb_url = $trackback_url;
	$url = urlencode(get_permalink($ID));
	$query_string = "title=$title&url=$url&blog_name=$blog_name&excerpt=$excerpt";
	$trackback_url = parse_url($trackback_url);
	$http_request  = 'POST ' . $trackback_url['path'] . ($trackback_url['query'] ? '?'.$trackback_url['query'] : '') . " HTTP/1.0\r\n";
	$http_request .= 'Host: '.$trackback_url['host']."\r\n";
	$http_request .= 'Content-Type: application/x-www-form-urlencoded; charset='.get_settings('blog_charset')."\r\n";
	$http_request .= 'Content-Length: '.strlen($query_string)."\r\n";
	$http_request .= "User-Agent: WordPress/" . $wp_version;
	$http_request .= "\r\n\r\n";
	$http_request .= $query_string;
	if ( '' == $trackback_url['port'] )
		$trackback_url['port'] = 80;
	$fs = @fsockopen($trackback_url['host'], $trackback_url['port'], $errno, $errstr, 4);
	@fputs($fs, $http_request);
/*
	$debug_file = 'trackback.log';
	$fp = fopen($debug_file, 'a');
	fwrite($fp, "\n*****\nRequest:\n\n$http_request\n\nResponse:\n\n");
	while(!@feof($fs)) {
		fwrite($fp, @fgets($fs, 4096));
	}
	fwrite($fp, "\n\n");
	fclose($fp);
*/
	@fclose($fs);

	$wpdb->query("UPDATE $wpdb->posts SET pinged = CONCAT(pinged, '\n', '$tb_url') WHERE ID = '$ID'");
	$wpdb->query("UPDATE $wpdb->posts SET to_ping = REPLACE(to_ping, '$tb_url', '') WHERE ID = '$ID'");
	return $result;
}

function make_url_footnote($content) {
	preg_match_all('/<a(.+?)href=\"(.+?)\"(.*?)>(.+?)<\/a>/', $content, $matches);
	$j = 0;
	for ($i=0; $i<count($matches[0]); $i++) {
		$links_summary = (!$j) ? "\n" : $links_summary;
		$j++;
		$link_match = $matches[0][$i];
		$link_number = '['.($i+1).']';
		$link_url = $matches[2][$i];
		$link_text = $matches[4][$i];
		$content = str_replace($link_match, $link_text.' '.$link_number, $content);
		$link_url = (strtolower(substr($link_url,0,7)) != 'http://') ? get_settings('home') . $link_url : $link_url;
		$links_summary .= "\n".$link_number.' '.$link_url;
	}
	$content = strip_tags($content);
	$content .= $links_summary;
	return $content;
}


function xmlrpc_getposttitle($content) {
	global $post_default_title;
	if (preg_match('/<title>(.+?)<\/title>/is', $content, $matchtitle)) {
		$post_title = $matchtitle[0];
		$post_title = preg_replace('/<title>/si', '', $post_title);
		$post_title = preg_replace('/<\/title>/si', '', $post_title);
	} else {
		$post_title = $post_default_title;
	}
	return $post_title;
}
	
function xmlrpc_getpostcategory($content) {
	global $post_default_category;
	if (preg_match('/<category>(.+?)<\/category>/is', $content, $matchcat)) {
		$post_category = trim($matchcat[1], ',');
		$post_category = explode(',', $post_category);
	} else {
		$post_category = $post_default_category;
	}
	return $post_category;
}

function xmlrpc_removepostdata($content) {
	$content = preg_replace('/<title>(.+?)<\/title>/si', '', $content);
	$content = preg_replace('/<category>(.+?)<\/category>/si', '', $content);
	$content = trim($content);
	return $content;
}

function debug_fopen($filename, $mode) {
	global $debug;
	if ($debug == 1) {
		$fp = fopen($filename, $mode);
		return $fp;
	} else {
		return false;
	}
}

function debug_fwrite($fp, $string) {
	global $debug;
	if ($debug == 1) {
		fwrite($fp, $string);
	}
}

function debug_fclose($fp) {
	global $debug;
	if ($debug == 1) {
		fclose($fp);
	}
}

function do_enclose( $content, $post_ID ) {
	global $wp_version, $wpdb;
	include_once (ABSPATH . WPINC . '/class-IXR.php');

	$log = debug_fopen(ABSPATH . '/enclosures.log', 'a');
	$post_links = array();
	debug_fwrite($log, 'BEGIN '.date('YmdHis', time())."\n");

	$pung = get_enclosed( $post_ID );

	$ltrs = '\w';
	$gunk = '/#~:.?+=&%@!\-';
	$punc = '.:?\-';
	$any = $ltrs . $gunk . $punc;

	preg_match_all("{\b http : [$any] +? (?= [$punc] * [^$any] | $)}x", $content, $post_links_temp);

	debug_fwrite($log, 'Post contents:');
	debug_fwrite($log, $content."\n");

	foreach($post_links_temp[0] as $link_test) :
		if ( !in_array($link_test, $pung) ) : // If we haven't pung it already
			$test = parse_url($link_test);
			if (isset($test['query']))
				$post_links[] = $link_test;
			elseif(($test['path'] != '/') && ($test['path'] != ''))
				$post_links[] = $link_test;
		endif;
	endforeach;

	foreach ($post_links as $url) :
		if ( $url != '' && !$wpdb->get_var("SELECT post_id FROM $wpdb->postmeta WHERE post_id = '$post_ID' AND meta_key = 'enclosure' AND meta_value LIKE ('$url%')") ) {
			if ( $headers = wp_get_http_headers( $url) ) {
				$len  = (int) $headers['content-length'];
				$type = addslashes( $headers['content-type'] );
				$allowed_types = array( 'video', 'audio' );
				if( in_array( substr( $type, 0, strpos( $type, "/" ) ), $allowed_types ) ) {
					$meta_value = "$url\n$len\n$type\n";
					$wpdb->query( "INSERT INTO `$wpdb->postmeta` ( `post_id` , `meta_key` , `meta_value` )
					VALUES ( '$post_ID', 'enclosure' , '$meta_value')" );
				}
			}
		}
	endforeach;
}

function wp_get_http_headers( $url ) {
	set_time_limit( 60 ); 
	$parts = parse_url( $url );
	$file  = $parts['path'] . ($parts['query'] ? '?'.$parts['query'] : '');
	$host  = $parts['host'];
	if ( !isset( $parts['port'] ) )
		$parts['port'] = 80;

	$head = "HEAD $file HTTP/1.1\r\nHOST: $host\r\n\r\n";

	$fp = @fsockopen($host, $parts['port'], $err_num, $err_msg, 3);
	if ( !$fp )
		return false;

	$response = '';
	fputs( $fp, $head );
	while ( !feof( $fp ) && strpos( $response, "\r\n\r\n" ) == false )
		$response .= fgets( $fp, 2048 );
	fclose( $fp );
	preg_match_all('/(.*?): (.*)\r/', $response, $matches);
	$count = count($matches[1]);
	for ( $i = 0; $i < $count; $i++) {
		$key = strtolower($matches[1][$i]);
		$headers["$key"] = $matches[2][$i];
	}

	preg_match('/.*([0-9]{3}).*/', $response, $return);
	$headers['response'] = $return[1]; // HTTP response code eg 204, 200, 404
	return $headers;
}

// Deprecated.  Use the new post loop.
function start_wp() {
	global $wp_query, $post;

	// Since the old style loop is being used, advance the query iterator here.
	$wp_query->next_post();

	setup_postdata($post);
}

// Setup global post data.
function setup_postdata($post) {
  global $id, $postdata, $authordata, $day, $page, $pages, $multipage, $more, $numpages, $wp_query;
	global $pagenow;

	$id = $post->ID;

	$authordata = get_userdata($post->post_author);

	$day = mysql2date('d.m.y', $post->post_date);
	$currentmonth = mysql2date('m', $post->post_date);
	$numpages = 1;
	$page = get_query_var('page');
	if (!$page)
		$page = 1;
	if (is_single() || is_page())
		$more = 1;
	$content = $post->post_content;
	if (preg_match('/<!--nextpage-->/', $content)) {
		if ($page > 1)
			$more = 1;
		$multipage = 1;
		$content = str_replace("\n<!--nextpage-->\n", '<!--nextpage-->', $content);
		$content = str_replace("\n<!--nextpage-->", '<!--nextpage-->', $content);
		$content = str_replace("<!--nextpage-->\n", '<!--nextpage-->', $content);
		$pages = explode('<!--nextpage-->', $content);
		$numpages = count($pages);
	} else {
		$pages[0] = $post->post_content;
		$multipage = 0;
	}
	return true;
}

function is_new_day() {
	global $day, $previousday;
	if ($day != $previousday) {
		return(1);
	} else {
		return(0);
	}
}

// Filters: these are the core of WP's plugin architecture

function merge_filters($tag) {
	global $wp_filter;
	if (isset($wp_filter['all'])) {
		foreach ($wp_filter['all'] as $priority => $functions) {
			if (isset($wp_filter[$tag][$priority]))
				$wp_filter[$tag][$priority] = array_merge($wp_filter['all'][$priority], $wp_filter[$tag][$priority]);
			else
				$wp_filter[$tag][$priority] = array_merge($wp_filter['all'][$priority], array());
			$wp_filter[$tag][$priority] = array_unique($wp_filter[$tag][$priority]);
		}
	}

	if ( isset($wp_filter[$tag]) )
		ksort( $wp_filter[$tag] );
}

function apply_filters($tag, $string) {
	global $wp_filter;
	
	$args = array_slice(func_get_args(), 2);

	merge_filters($tag);
	
	if (!isset($wp_filter[$tag])) {
		return $string;
	}
	foreach ($wp_filter[$tag] as $priority => $functions) {
		if (!is_null($functions)) {
			foreach($functions as $function) {

				$all_args = array_merge(array($string), $args);
				$function_name = $function['function'];
				$accepted_args = $function['accepted_args'];

				if($accepted_args == 1) {
					$the_args = array($string);
				} elseif ($accepted_args > 1) {
					$the_args = array_slice($all_args, 0, $accepted_args);
				} elseif($accepted_args == 0) {
					$the_args = NULL;
				} else {
					$the_args = $all_args;
				}

				$string = call_user_func_array($function_name, $the_args);
			}
		}
	}
	return $string;
}

function add_filter($tag, $function_to_add, $priority = 10, $accepted_args = 1) {
	global $wp_filter;

	// check that we don't already have the same filter at the same priority
	if (isset($wp_filter[$tag]["$priority"])) {
		foreach($wp_filter[$tag]["$priority"] as $filter) {
			// uncomment if we want to match function AND accepted_args
			//if ($filter == array($function, $accepted_args)) {
			if ($filter['function'] == $function_to_add) {
				return true;
			}
		}
	}

	// So the format is wp_filter['tag']['array of priorities']['array of ['array (functions, accepted_args)]']
	$wp_filter[$tag]["$priority"][] = array('function'=>$function_to_add, 'accepted_args'=>$accepted_args);
	return true;
}

function remove_filter($tag, $function_to_remove, $priority = 10, $accepted_args = 1) {
	global $wp_filter;

	// rebuild the list of filters
	if (isset($wp_filter[$tag]["$priority"])) {
		foreach($wp_filter[$tag]["$priority"] as $filter) {
			if ($filter['function'] != $function_to_remove) {
				$new_function_list[] = $filter;
			}
		}
		$wp_filter[$tag]["$priority"] = $new_function_list;
	}
	return true;
}

// The *_action functions are just aliases for the *_filter functions, they take special strings instead of generic content

function do_action($tag, $arg = '') {
	global $wp_filter;
	$extra_args = array_slice(func_get_args(), 2);
 	if ( is_array($arg) )
 		$args = array_merge($arg, $extra_args);
	else
		$args = array_merge(array($arg), $extra_args);
	
	merge_filters($tag);
	
	if (!isset($wp_filter[$tag])) {
		return;
	}
	foreach ($wp_filter[$tag] as $priority => $functions) {
		if (!is_null($functions)) {
			foreach($functions as $function) {

				$function_name = $function['function'];
				$accepted_args = $function['accepted_args'];

				if($accepted_args == 1) {
					if ( is_array($arg) )
						$the_args = $arg;
					else
						$the_args = array($arg);
				} elseif ($accepted_args > 1) {
					$the_args = array_slice($args, 0, $accepted_args);
				} elseif($accepted_args == 0) {
					$the_args = NULL;
				} else {
					$the_args = $args;
				}

				$string = call_user_func_array($function_name, $the_args);
			}
		}
	}
}

function add_action($tag, $function_to_add, $priority = 10, $accepted_args = 1) {
	add_filter($tag, $function_to_add, $priority, $accepted_args);
}

function remove_action($tag, $function_to_remove, $priority = 10, $accepted_args = 1) {
	remove_filter($tag, $function_to_remove, $priority, $accepted_args);
}

function get_page_uri($page_id) {
	$page = get_page($page_id);
	$uri = urldecode($page->post_name);

	// A page cannot be it's own parent.
	if ($page->post_parent == $page->ID) {
		return $uri;
	}
	
	while ($page->post_parent != 0) {
		$page = get_page($page->post_parent);
		$uri = urldecode($page->post_name) . "/" . $uri;
	}

	return $uri;
}

function get_posts($args) {
	global $wpdb;
	parse_str($args, $r);
	if (!isset($r['numberposts'])) $r['numberposts'] = 5;
	if (!isset($r['offset'])) $r['offset'] = 0;
	if (!isset($r['category'])) $r['category'] = '';
	if (!isset($r['orderby'])) $r['orderby'] = 'post_date';
	if (!isset($r['order'])) $r['order'] = 'DESC';

	$now = current_time('mysql');

	$posts = $wpdb->get_results(
		"SELECT DISTINCT * FROM $wpdb->posts " .
		( empty( $r['category'] ) ? "" : ", $wpdb->post2cat " ) .
		" WHERE post_date <= '$now' AND (post_status = 'publish') ".
		( empty( $r['category'] ) ? "" : "AND $wpdb->posts.ID = $wpdb->post2cat.post_id AND $wpdb->post2cat.category_id = " . $r['category']. " " ) .
		" GROUP BY $wpdb->posts.ID ORDER BY " . $r['orderby'] . " " . $r['order'] . "  LIMIT " . $r['offset'] . ',' . $r['numberposts'] );
	
    update_post_caches($posts);
	
	return $posts;
}

function &query_posts($query) {
	global $wp_query;
	return $wp_query->query($query);
}

function update_post_cache(&$posts) {
	global $post_cache;

	if ( !$posts )
		return;

	for ($i = 0; $i < count($posts); $i++) {
		$post_cache[$posts[$i]->ID] = &$posts[$i];
	}
}

function update_page_cache(&$pages) {
	global $page_cache;

	if ( !$pages )
		return;

	for ($i = 0; $i < count($pages); $i++) {
		$page_cache[$pages[$i]->ID] = &$pages[$i];
	}
}

function update_post_category_cache($post_ids) {
	global $wpdb, $category_cache, $cache_categories;

	if (empty($post_ids))
		return;

	if (is_array($post_ids))
		$post_ids = implode(',', $post_ids);

	$dogs = $wpdb->get_results("SELECT DISTINCT
	post_id, cat_ID FROM $wpdb->categories, $wpdb->post2cat
	WHERE category_id = cat_ID AND post_id IN ($post_ids)");

	if (! isset($cache_categories))
		update_category_cache();
		
	if ( !empty($dogs) ) {
		foreach ($dogs as $catt) {
			$category_cache[$catt->post_id][$catt->cat_ID] = &$cache_categories[$catt->cat_ID];
		}
	}
}

function update_post_caches(&$posts) {
	global $post_cache, $category_cache, $comment_count_cache, $post_meta_cache;
	global $wpdb;
	
	// No point in doing all this work if we didn't match any posts.
	if ( !$posts )
		return;

	// Get the categories for all the posts
	for ($i = 0; $i < count($posts); $i++) {
		$post_id_list[] = $posts[$i]->ID;
		$post_cache[$posts[$i]->ID] = &$posts[$i];
	}

	$post_id_list = implode(',', $post_id_list);
	
	update_post_category_cache($post_id_list);

	// Do the same for comment numbers
	$comment_counts = $wpdb->get_results("SELECT ID, COUNT( comment_ID ) AS ccount
	FROM $wpdb->posts
	LEFT JOIN $wpdb->comments ON ( comment_post_ID = ID  AND comment_approved =  '1')
	WHERE ID IN ($post_id_list)
	GROUP BY ID");
	
	if ($comment_counts) {
		foreach ($comment_counts as $comment_count)
			$comment_count_cache["$comment_count->ID"] = $comment_count->ccount;
	}

    // Get post-meta info
	if ( $meta_list = $wpdb->get_results("SELECT post_id, meta_key, meta_value FROM $wpdb->postmeta  WHERE post_id IN($post_id_list) ORDER BY post_id, meta_key", ARRAY_A) ) {
		// Change from flat structure to hierarchical:
		$post_meta_cache = array();
		foreach ($meta_list as $metarow) {
			$mpid = $metarow['post_id'];
			$mkey = $metarow['meta_key'];
			$mval = $metarow['meta_value'];

			// Force subkeys to be array type:
			if (!isset($post_meta_cache[$mpid]) || !is_array($post_meta_cache[$mpid]))
				$post_meta_cache[$mpid] = array();
			if (!isset($post_meta_cache[$mpid]["$mkey"]) || !is_array($post_meta_cache[$mpid]["$mkey"]))
				$post_meta_cache[$mpid]["$mkey"] = array();

			// Add a value to the current pid/key:
			$post_meta_cache[$mpid][$mkey][] = $mval;
		}
	}
}

function update_category_cache() {
	global $cache_categories, $wpdb;
	$dogs = $wpdb->get_results("SELECT * FROM $wpdb->categories");
	foreach ($dogs as $catt)
		$cache_categories[$catt->cat_ID] = $catt;
}

function update_user_cache() {
	global $cache_userdata, $wpdb;
	
	if ( $users = $wpdb->get_results("SELECT * FROM $wpdb->users WHERE user_level > 0") ) :
		foreach ($users as $user) :
			$cache_userdata[$user->ID] = $user;
			$cache_userdata[$user->user_login] =& $cache_userdata[$user->ID];
		endforeach;
		return true;
	else : 
		return false;
	endif;
}

function wp_head() {
	do_action('wp_head');
}

function wp_footer() {
	do_action('wp_footer');
}

function is_single ($post = '') {
	global $wp_query;

	if ( !$wp_query->is_single )
		return false;

	if ( empty( $post) )
		return true;

	$post_obj = $wp_query->get_queried_object();

	if ( $post == $post_obj->ID ) 
		return true;
	elseif ( $post == $post_obj->post_title ) 
		return true;
	elseif ( $post == $post_obj->post_name )
		return true;

	return false;
}

function is_page ($page = '') {
	global $wp_query;

	if (! $wp_query->is_page) {
		return false;
	}

	if (empty($page)) {
		return true;
	}

	$page_obj = $wp_query->get_queried_object();
		
	if ($page == $page_obj->ID) {
		return true;
	} else if ($page == $page_obj->post_title) {
		return true;
	} else if ($page == $page_obj->post_name) {
		return true;
	}

	return false;
}

function is_archive () {
    global $wp_query;

    return $wp_query->is_archive;
}

function is_date () {
    global $wp_query;

    return $wp_query->is_date;
}

function is_year () {
    global $wp_query;

    return $wp_query->is_year;
}

function is_month () {
    global $wp_query;

    return $wp_query->is_month;
}

function is_day () {
    global $wp_query;

    return $wp_query->is_day;
}

function is_time () {
    global $wp_query;

    return $wp_query->is_time;
}

function is_author ($author = '') {
	global $wp_query;

	if (! $wp_query->is_author) {
		return false;
	}

	if (empty($author)) {
		return true;
	}

	$author_obj = $wp_query->get_queried_object();
		
	if ($author == $author_obj->ID) {
		return true;
	} else if ($author == $author_obj->user_nickname) {
		return true;
	} else if ($author == $author_obj->user_nicename) {
		return true;
	}

	return false;
}

function is_category ($category = '') {
	global $wp_query;

	if (! $wp_query->is_category) {
		return false;
	}

	if (empty($category)) {
		return true;
	}

	$cat_obj = $wp_query->get_queried_object();
		
	if ($category == $cat_obj->cat_ID) {
		return true;
	} else if ($category == $cat_obj->cat_name) {
		return true;
	} else if ($category == $cat_obj->category_nicename) {
		return true;
	}

	return false;
}

function is_search () {
    global $wp_query;

    return $wp_query->is_search;
}

function is_feed () {
    global $wp_query;

    return $wp_query->is_feed;
}

function is_trackback () {
    global $wp_query;

    return $wp_query->is_trackback;
}

function is_admin () {
    global $wp_query;

    return $wp_query->is_admin;
}

function is_home () {
    global $wp_query;

    return $wp_query->is_home;
}

function is_404 () {
    global $wp_query;

    return $wp_query->is_404;
}

function is_comments_popup () {
    global $wp_query;

    return $wp_query->is_comments_popup;
}

function is_paged () {
    global $wp_query;

    return $wp_query->is_paged;
}

function get_query_var($var) {
  global $wp_query;

  return $wp_query->get($var);
}

function have_posts() {
    global $wp_query;

    return $wp_query->have_posts();
}

function rewind_posts() {
    global $wp_query;

    return $wp_query->rewind_posts();
}

function the_post() {
    global $wp_query;
    $wp_query->the_post();
}

function get_theme_root() {
	return apply_filters('theme_root', ABSPATH . "wp-content/themes");
}

function get_theme_root_uri() {
	return apply_filters('theme_root_uri', get_settings('siteurl') . "/wp-content/themes", get_settings('siteurl'));
}

function get_stylesheet() {
	return apply_filters('stylesheet', get_settings('stylesheet'));
}

function get_stylesheet_directory() {
	$stylesheet = get_stylesheet();
	$stylesheet_dir = get_theme_root() . "/$stylesheet";
	return apply_filters('stylesheet_directory', $stylesheet_dir, $stylesheet);
}

function get_stylesheet_directory_uri() {
	$stylesheet = get_stylesheet();
	$stylesheet_dir_uri = get_theme_root_uri() . "/$stylesheet";
	return apply_filters('stylesheet_directory_uri', $stylesheet_dir_uri, $stylesheet);
}

function get_stylesheet_uri() {
	$stylesheet_dir_uri = get_stylesheet_directory_uri();
	$stylesheet_uri = $stylesheet_dir_uri . "/style.css";
	return apply_filters('stylesheet_uri', $stylesheet_uri, $stylesheet_dir_uri);
}

function get_template() {
	return apply_filters('template', get_settings('template'));
}

function get_template_directory() {
	$template = get_template();
	$template_dir = get_theme_root() . "/$template";
	return apply_filters('template_directory', $template_dir, $template);
}

function get_template_directory_uri() {
	$template = get_template();
	$template_dir_uri = get_theme_root_uri() . "/$template";
	return apply_filters('template_directory_uri', $template_dir_uri, $template);
}

function get_theme_data($theme_file) {
	$theme_data = implode('', file($theme_file));
	preg_match("|Theme Name:(.*)|i", $theme_data, $theme_name);
	preg_match("|Theme URI:(.*)|i", $theme_data, $theme_uri);
	preg_match("|Description:(.*)|i", $theme_data, $description);
	preg_match("|Author:(.*)|i", $theme_data, $author_name);
	preg_match("|Author URI:(.*)|i", $theme_data, $author_uri);
	preg_match("|Template:(.*)|i", $theme_data, $template);
	if ( preg_match("|Version:(.*)|i", $theme_data, $version) )
		$version = $version[1];
	else
		$version ='';
	if ( preg_match("|Status:(.*)|i", $theme_data, $status) )
		$status = $status[1];
	else
		$status ='publish';

	$description = wptexturize($description[1]);

	$name = $theme_name[1];
	$name = trim($name);
	$theme = $name;
	if ('' != $theme_uri[1] && '' != $name) {
		$theme = '<a href="' . $theme_uri[1] . '" title="' . __('Visit theme homepage') . '">' . $theme . '</a>';
	}

	if ('' == $author_uri[1]) {
		$author = $author_name[1];
	} else {
		$author = '<a href="' . $author_uri[1] . '" title="' . __('Visit author homepage') . '">' . $author_name[1] . '</a>';
	}

	return array('Name' => $name, 'Title' => $theme, 'Description' => $description, 'Author' => $author, 'Version' => $version, 'Template' => $template[1], 'Status' => $status);
}

function get_themes() {
	global $wp_themes;
	global $wp_broken_themes;

	if (isset($wp_themes)) {
		return $wp_themes;
	}

	$themes = array();
	$wp_broken_themes = array();
	$theme_root = get_theme_root();
	$theme_loc = str_replace(ABSPATH, '', $theme_root);

	// Files in wp-content/themes directory
	$themes_dir = @ dir($theme_root);
	if ($themes_dir) {
		while(($theme_dir = $themes_dir->read()) !== false) {
			if (is_dir($theme_root . '/' . $theme_dir)) {
				if ($theme_dir{0} == '.' || $theme_dir == '..' || $theme_dir == 'CVS') {
					continue;
				}
				$stylish_dir = @ dir($theme_root . '/' . $theme_dir);
				$found_stylesheet = false;
				while(($theme_file = $stylish_dir->read()) !== false) {
					if ( $theme_file == 'style.css' ) {
						$theme_files[] = $theme_dir . '/' . $theme_file;
						$found_stylesheet = true;
						break;
					}
				}
				if (!$found_stylesheet) {
					$wp_broken_themes[$theme_dir] = array('Name' => $theme_dir, 'Title' => $theme_dir, 'Description' => __('Stylesheet is missing.'));
				}
			}
		}
	}

	if (!$themes_dir || !$theme_files) {
		return $themes;
	}

	sort($theme_files);

	foreach($theme_files as $theme_file) {
		$theme_data = get_theme_data("$theme_root/$theme_file");
	  
		$name = $theme_data['Name']; 
		$title = $theme_data['Title'];
		$description = wptexturize($theme_data['Description']);
		$version = $theme_data['Version'];
		$author = $theme_data['Author'];
		$template = $theme_data['Template'];
		$stylesheet = dirname($theme_file);

		if (empty($name)) {
			$name = dirname($theme_file);
			$title = $name;
		}

		if (empty($template)) {
			if (file_exists(dirname("$theme_root/$theme_file/index.php"))) {
				$template = dirname($theme_file);
			} else {
				continue;
			}
		}

		$template = trim($template);

		if (! file_exists("$theme_root/$template/index.php")) {
			$wp_broken_themes[$name] = array('Name' => $name, 'Title' => $title, 'Description' => __('Template is missing.'));
			continue;
		}
		
		$stylesheet_files = array();
		$stylesheet_dir = @ dir("$theme_root/$stylesheet");
		if ($stylesheet_dir) {
			while(($file = $stylesheet_dir->read()) !== false) {
				if ( !preg_match('|^\.+$|', $file) && preg_match('|\.css$|', $file) ) 
					$stylesheet_files[] = "$theme_loc/$stylesheet/$file";
			}
		}

		$template_files = array();		
		$template_dir = @ dir("$theme_root/$template");
		if ($template_dir) {
			while(($file = $template_dir->read()) !== false) {
				if ( !preg_match('|^\.+$|', $file) && preg_match('|\.php$|', $file) ) 
					$template_files[] = "$theme_loc/$template/$file";
			}
		}

		$template_dir = dirname($template_files[0]);
		$stylesheet_dir = dirname($stylesheet_files[0]);

		if (empty($template_dir)) $template_dir = '/';
		if (empty($stylesheet_dir)) $stylesheet_dir = '/';

		// Check for theme name collision.  This occurs if a theme is copied to
		// a new theme directory and the theme header is not updated.  Whichever
		// theme is first keeps the name.  Subsequent themes get a suffix applied.
		// The Default and Classic themes always trump their pretenders.
		if ( isset($themes[$name]) ) {
			if ( ('WordPress Default' == $name || 'WordPress Classic' == $name) &&
					 ('default' == $stylesheet || 'classic' == $stylesheet) ) {
				// If another theme has claimed to be one of our default themes, move
				// them aside.
				$suffix = $themes[$name]['Stylesheet'];
				$new_name = "$name/$suffix";
				$themes[$new_name] = $themes[$name];
				$themes[$new_name]['Name'] = $new_name;
			} else {
				$name = "$name/$stylesheet";
			}
		}
		
		$themes[$name] = array('Name' => $name, 'Title' => $title, 'Description' => $description, 'Author' => $author, 'Version' => $version, 'Template' => $template, 'Stylesheet' => $stylesheet, 'Template Files' => $template_files, 'Stylesheet Files' => $stylesheet_files, 'Template Dir' => $template_dir, 'Stylesheet Dir' => $stylesheet_dir, 'Status' => $theme_data['Status']);
	}

	// Resolve theme dependencies.
	$theme_names = array_keys($themes);

	foreach ($theme_names as $theme_name) {
		$themes[$theme_name]['Parent Theme'] = '';
		if ($themes[$theme_name]['Stylesheet'] != $themes[$theme_name]['Template']) {
			foreach ($theme_names as $parent_theme_name) {
				if (($themes[$parent_theme_name]['Stylesheet'] == $themes[$parent_theme_name]['Template']) && ($themes[$parent_theme_name]['Template'] == $themes[$theme_name]['Template'])) {
					$themes[$theme_name]['Parent Theme'] = $themes[$parent_theme_name]['Name'];
					break;
				}
			}
		}
	}

	$wp_themes = $themes;

	return $themes;
}

function get_theme($theme) {
	$themes = get_themes();

	if (array_key_exists($theme, $themes)) {
		return $themes[$theme];
	}

	return NULL;
}

function get_current_theme() {
	$themes = get_themes();
	$theme_names = array_keys($themes);
	$current_template = get_settings('template');
	$current_stylesheet = get_settings('stylesheet');
	$current_theme = 'WordPress Default';

	if ($themes) {
		foreach ($theme_names as $theme_name) {
			if ($themes[$theme_name]['Stylesheet'] == $current_stylesheet &&
					$themes[$theme_name]['Template'] == $current_template) {
				$current_theme = $themes[$theme_name]['Name'];
				break;
			}
		}
	}

	return $current_theme;
}

function get_query_template($type) {
	$template = '';
	if ( file_exists(TEMPLATEPATH . "/{$type}.php") )
		$template = TEMPLATEPATH . "/{$type}.php";

	return apply_filters("{$type}_template", $template);
}

function get_404_template() {
	return get_query_template('404');
}

function get_archive_template() {
	return get_query_template('archive');
}

function get_author_template() {
	return get_query_template('author');
}

function get_category_template() {
	$template = '';
	if ( file_exists(TEMPLATEPATH . "/category-" . get_query_var('cat') . '.php') )
		$template = TEMPLATEPATH . "/category-" . get_query_var('cat') . '.php';
	else if ( file_exists(TEMPLATEPATH . "/category.php") )
		$template = TEMPLATEPATH . "/category.php";

	return apply_filters('category_template', $template);
}

function get_date_template() {
	return get_query_template('date');
}

function get_home_template() {
	$template = '';

	if ( file_exists(TEMPLATEPATH . "/home.php") )
		$template = TEMPLATEPATH . "/home.php";
	else if ( file_exists(TEMPLATEPATH . "/index.php") )
		$template = TEMPLATEPATH . "/index.php";

	return apply_filters('home_template', $template);
}

function get_page_template() {
	global $wp_query;

	$id = $wp_query->post->ID;	
	$template = get_post_meta($id, '_wp_page_template', true);

	if ( 'default' == $template )
		$template = '';

	if ( ! empty($template) && file_exists(TEMPLATEPATH . "/$template") )
		$template = TEMPLATEPATH . "/$template";
	else if ( file_exists(TEMPLATEPATH .  "/page.php") )
		$template = TEMPLATEPATH .  "/page.php";
	else
		$template = '';

	return apply_filters('page_template', $template);
}

function get_paged_template() {
	return get_query_template('paged');
}

function get_search_template() {
	return get_query_template('search');
}

function get_single_template() {
	return get_query_template('single');
}

function get_comments_popup_template() {
	if ( file_exists( TEMPLATEPATH . '/comments-popup.php') )
		$template = TEMPLATEPATH . '/comments-popup.php';
	else
		$template = get_theme_root() . '/default/comments-popup.php';

	return apply_filters('comments_popup_template', $template);
}

// Borrowed from the PHP Manual user notes. Convert entities, while
// preserving already-encoded entities:
function htmlentities2($myHTML) {
	$translation_table=get_html_translation_table (HTML_ENTITIES,ENT_QUOTES);
	$translation_table[chr(38)] = '&';
	return preg_replace("/&(?![A-Za-z]{0,4}\w{2,3};|#[0-9]{2,3};)/","&amp;" , strtr($myHTML, $translation_table));
}


function is_plugin_page() {
	global $plugin_page;

	if (isset($plugin_page)) {
		return true;
	}

	return false;
}

/*
add_query_arg: Returns a modified querystring by adding
a single key & value or an associative array.
Setting a key value to emptystring removes the key.
Omitting oldquery_or_uri uses the $_SERVER value.

Parameters:
add_query_arg(newkey, newvalue, oldquery_or_uri) or
add_query_arg(associative_array, oldquery_or_uri)
*/
function add_query_arg() {
	$ret = '';
	if(is_array(func_get_arg(0))) {
		$uri = @func_get_arg(1);
	}
	else {
		if (@func_num_args() < 3) {
			$uri = $_SERVER['REQUEST_URI'];
		} else {
			$uri = @func_get_arg(2);
		}
	}

	if (strstr($uri, '?')) {
		$parts = explode('?', $uri, 2);
		if (1 == count($parts)) {
			$base = '?';
			$query = $parts[0];
		}
		else {
			$base = $parts[0] . '?';
			$query = $parts[1];
		}
	}
	else if (strstr($uri, '/')) {
		$base = $uri . '?';
		$query = '';
	} else {
		$base = '';
		$query = $uri;
	}

	parse_str($query, $qs);
	if (is_array(func_get_arg(0))) {
		$kayvees = func_get_arg(0);
		$qs = array_merge($qs, $kayvees);
	}
	else
    {
			$qs[func_get_arg(0)] = func_get_arg(1);
    }

	foreach($qs as $k => $v)
    {
			if($v != '')
        {
					if($ret != '') $ret .= '&';
					$ret .= "$k=$v";
        }
    }
	$ret = $base . $ret;   
	return trim($ret, '?');
}

function remove_query_arg($key, $query) {
	add_query_arg($key, '', $query);
}

function load_template($file) {
	global $posts, $post, $wp_did_header, $wp_did_template_redirect, $wp_query,
		$wp_rewrite, $wpdb;

	extract($wp_query->query_vars);

	require_once($file);
}

function add_magic_quotes($array) {
	foreach ($array as $k => $v) {
		if (is_array($v)) {
			$array[$k] = add_magic_quotes($v);
		} else {
			$array[$k] = addslashes($v);
		}
	}
	return $array;
}

function wp_remote_fopen( $uri ) {
	if ( ini_get('allow_url_fopen') ) {
		$fp = fopen( $uri, 'r' );
		if ( !$fp )
			return false;
		$linea = '';
		while( $remote_read = fread($fp, 4096) )
			$linea .= $remote_read;
		fclose($fp);
		return $linea;		
	} else if ( function_exists('curl_init') ) {
		$handle = curl_init();
		curl_setopt ($handle, CURLOPT_URL, $uri);
		curl_setopt ($handle, CURLOPT_CONNECTTIMEOUT, 1);
		curl_setopt ($handle, CURLOPT_RETURNTRANSFER, 1);
		$buffer = curl_exec($handle);
		curl_close($handle);
		return $buffer;
	} else {
		return false;
	}	
}

?>