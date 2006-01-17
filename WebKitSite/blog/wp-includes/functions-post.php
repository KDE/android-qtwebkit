<?php

/**** DB Functions ****/

/*
 * generic function for inserting data into the posts table.
 */
function wp_insert_post($postarr = array()) {
	global $wpdb, $post_default_category, $allowedtags;
	
	// export array as variables
	extract($postarr);
	
	// Do some escapes for safety
	$post_title = $wpdb->escape($post_title);
	$post_name = sanitize_title($post_title);
	$post_excerpt = $wpdb->escape($post_excerpt);
	$post_content = $wpdb->escape($post_content);
	$post_author = (int) $post_author;

	// Make sure we set a valid category
	if (0 == count($post_category) || !is_array($post_category)) {
		$post_category = array($post_default_category);
	}

	$post_cat = $post_category[0];
	
	if (empty($post_date))
		$post_date = current_time('mysql');
	// Make sure we have a good gmt date:
	if (empty($post_date_gmt)) 
		$post_date_gmt = get_gmt_from_date($post_date);
	if (empty($comment_status))
		$comment_status = get_settings('default_comment_status');
	if (empty($ping_status))
		$ping_status = get_settings('default_ping_status');
	if ( empty($post_parent) )
		$post_parent = 0;

	if ('publish' == $post_status) {
		$post_name_check = $wpdb->get_var("SELECT post_name FROM $wpdb->posts WHERE post_name = '$post_name' AND post_status = 'publish' AND ID != '$post_ID' LIMIT 1");
		if ($post_name_check) {
			$suffix = 2;
			while ($post_name_check) {
				$alt_post_name = $post_name . "-$suffix";
				$post_name_check = $wpdb->get_var("SELECT post_name FROM $wpdb->posts WHERE post_name = '$alt_post_name' AND post_status = 'publish' AND ID != '$post_ID' LIMIT 1");
				$suffix++;
			}
			$post_name = $alt_post_name;
		}
	}

	$sql = "INSERT INTO $wpdb->posts 
		(post_author, post_date, post_date_gmt, post_modified, post_modified_gmt, post_content, post_title, post_excerpt, post_category, post_status, post_name, comment_status, ping_status, post_parent) 
		VALUES ('$post_author', '$post_date', '$post_date_gmt', '$post_date', '$post_date_gmt', '$post_content', '$post_title', '$post_excerpt', '$post_cat', '$post_status', '$post_name', '$comment_status', '$ping_status', '$post_parent')";
	
	$result = $wpdb->query($sql);
	$post_ID = $wpdb->insert_id;

	// Set GUID
	$wpdb->query("UPDATE $wpdb->posts SET guid = '" . get_permalink($post_ID) . "' WHERE ID = '$post_ID'");
	
	wp_set_post_cats('', $post_ID, $post_category);
	
	if ($post_status == 'publish') {
		do_action('publish_post', $post_ID);
	}

	pingback($content, $post_ID);

	// Return insert_id if we got a good result, otherwise return zero.
	return $result ? $post_ID : 0;
}

function wp_get_single_post($postid = 0, $mode = OBJECT) {
	global $wpdb;

	$sql = "SELECT * FROM $wpdb->posts WHERE ID=$postid";
	$result = $wpdb->get_row($sql, $mode);
	
	// Set categories
	if($mode == OBJECT) {
		$result->post_category = wp_get_post_cats('',$postid);
	} 
	else {
		$result['post_category'] = wp_get_post_cats('',$postid);
	}

	return $result;
}

function wp_get_recent_posts($num = 10) {
	global $wpdb;

	// Set the limit clause, if we got a limit
	if ($num) {
		$limit = "LIMIT $num";
	}

	$sql = "SELECT * FROM $wpdb->posts WHERE post_status IN ('publish', 'draft', 'private') ORDER BY post_date DESC $limit";
	$result = $wpdb->get_results($sql,ARRAY_A);

	return $result?$result:array();
}

function wp_update_post($postarr = array()) {
	global $wpdb;

	// First get all of the original fields
	extract(wp_get_single_post($postarr['ID'], ARRAY_A));	

	// Now overwrite any changed values being passed in
	extract($postarr);

	// Make sure we set a valid category
	if ( 0 == count($post_category) || !is_array($post_category) )
		$post_category = array($post_default_category);

	// Do some escapes for safety
	$post_title   = $wpdb->escape($post_title);
	$post_excerpt = $wpdb->escape($post_excerpt);
	$post_content = $wpdb->escape($post_content);

	$post_modified = current_time('mysql');
	$post_modified_gmt = current_time('mysql', 1);

	$sql = "UPDATE $wpdb->posts 
		SET post_content = '$post_content',
		post_title = '$post_title',
		post_category = $post_category[0],
		post_status = '$post_status',
		post_date = '$post_date',
		post_date_gmt = '$post_date_gmt',
		post_modified = '$post_modified',
		post_modified_gmt = '$post_modified_gmt',
		post_excerpt = '$post_excerpt',
		ping_status = '$ping_status',
		comment_status = '$comment_status'
		WHERE ID = $ID";
		
	$result = $wpdb->query($sql);
	$rows_affected = $wpdb->rows_affected;

	wp_set_post_cats('', $ID, $post_category);

	do_action('edit_post', $ID);

	return $rows_affected;
}

function wp_get_post_cats($blogid = '1', $post_ID = 0) {
	global $wpdb;
	
	$sql = "SELECT category_id 
		FROM $wpdb->post2cat 
		WHERE post_id = $post_ID 
		ORDER BY category_id";

	$result = $wpdb->get_col($sql);

	return array_unique($result);
}

function wp_set_post_cats($blogid = '1', $post_ID = 0, $post_categories = array()) {
	global $wpdb;
	// If $post_categories isn't already an array, make it one:
	if (!is_array($post_categories)) {
		if (!$post_categories) {
			$post_categories = 1;
		}
		$post_categories = array($post_categories);
	}

	$post_categories = array_unique($post_categories);

	// First the old categories
	$old_categories = $wpdb->get_col("
		SELECT category_id 
		FROM $wpdb->post2cat 
		WHERE post_id = $post_ID");
	
	if (!$old_categories) {
		$old_categories = array();
	} else {
		$old_categories = array_unique($old_categories);
	}


	$oldies = printr($old_categories,1);
	$newbies = printr($post_categories,1);

	// Delete any?
	$delete_cats = array_diff($old_categories,$post_categories);

	if ($delete_cats) {
		foreach ($delete_cats as $del) {
			$wpdb->query("
				DELETE FROM $wpdb->post2cat 
				WHERE category_id = $del 
					AND post_id = $post_ID 
				");
		}
	}

	// Add any?
	$add_cats = array_diff($post_categories, $old_categories);

	if ($add_cats) {
		foreach ($add_cats as $new_cat) {
			$wpdb->query("
				INSERT INTO $wpdb->post2cat (post_id, category_id) 
				VALUES ($post_ID, $new_cat)");
		}
	}
}	// wp_set_post_cats()

function wp_delete_post($postid = 0) {
	global $wpdb;
	$postid = (int) $postid;

	if ( !$post = $wpdb->get_row("SELECT * FROM $wpdb->posts WHERE ID = $postid") )
		return $post;

	if ( 'static' == $post->post_status )
		$wpdb->query("UPDATE $wpdb->posts SET post_parent = $post->post_parent WHERE post_parent = $postid AND post_status = 'static'");

	$wpdb->query("DELETE FROM $wpdb->posts WHERE ID = $postid");
	
	$wpdb->query("DELETE FROM $wpdb->comments WHERE comment_post_ID = $postid");

	$wpdb->query("DELETE FROM $wpdb->post2cat WHERE post_id = $postid");

	$wpdb->query("DELETE FROM $wpdb->postmeta WHERE post_id = $postid");
	
	return $post;
}

/**** /DB Functions ****/

/**** Misc ****/

// get permalink from post ID
function post_permalink($post_id = 0, $mode = '') { // $mode legacy
	return get_permalink($post_id);
}

// Get the name of a category from its ID
function get_cat_name($cat_id) {
	global $wpdb;
	
	$cat_id -= 0; 	// force numeric
	$name = $wpdb->get_var("SELECT cat_name FROM $wpdb->categories WHERE cat_ID=$cat_id");
	
	return $name;
}

// Get the ID of a category from its name
function get_cat_ID($cat_name='General') {
	global $wpdb;
	
	$cid = $wpdb->get_var("SELECT cat_ID FROM $wpdb->categories WHERE cat_name='$cat_name'");

	return $cid?$cid:1;	// default to cat 1
}

// Get author's preferred display name
function get_author_name( $auth_id ) {
	$authordata = get_userdata( $auth_id );

	switch( $authordata['user_idmode'] ) {
		case 'nickname':
			$authorname = $authordata['user_nickname'];
			break;
		case 'login':
			$authorname = $authordata['user_login'];
			break;
		case 'firstname':
			$authorname = $authordata['user_firstname'];
			break;
		case 'lastname':
			$authorname = $authordata['user_lastname'];
			break;
		case 'namefl':
			$authorname = $authordata['user_firstname'].' '.$authordata['user_lastname'];
			break;
		case 'namelf':
			$authorname = $authordata['user_lastname'].' '.$authordata['user_firstname'];
			break;
		default:
			$authorname = $authordata['user_nickname'];
			break;
	}

	return $authorname;
}

// get extended entry info (<!--more-->)
function get_extended($post) {
	list($main,$extended) = explode('<!--more-->', $post, 2);

	// Strip leading and trailing whitespace
	$main = preg_replace('/^[\s]*(.*)[\s]*$/','\\1',$main);
	$extended = preg_replace('/^[\s]*(.*)[\s]*$/','\\1',$extended);

	return array('main' => $main, 'extended' => $extended);
}

// do trackbacks for a list of urls
// borrowed from edit.php
// accepts a comma-separated list of trackback urls and a post id
function trackback_url_list($tb_list, $post_id) {
	if (!empty($tb_list)) {
		// get post data
		$postdata = wp_get_single_post($post_id, ARRAY_A);

		// import postdata as variables
		extract($postdata);
		
		// form an excerpt
		$excerpt = strip_tags($post_excerpt?$post_excerpt:$post_content);
		
		if (strlen($excerpt) > 255) {
			$excerpt = substr($excerpt,0,252) . '...';
		}
		
		$trackback_urls = explode(',', $tb_list);
		foreach($trackback_urls as $tb_url) {
		    $tb_url = trim($tb_url);
		    trackback($tb_url, stripslashes($post_title), $excerpt, $post_id);
		}
    }
}


// query user capabilities
// rather simplistic. shall evolve with future permission system overhaul
// $blog_id and $category_id are there for future usage

/* returns true if $user_id can create a new post */
function user_can_create_post($user_id, $blog_id = 1, $category_id = 'None') {
	$author_data = get_userdata($user_id);
	return ($author_data->user_level > 1);
}

/* returns true if $user_id can create a new post */
function user_can_create_draft($user_id, $blog_id = 1, $category_id = 'None') {
	$author_data = get_userdata($user_id);
	return ($author_data->user_level >= 1);
}

/* returns true if $user_id can edit $post_id */
function user_can_edit_post($user_id, $post_id, $blog_id = 1) {
	$author_data = get_userdata($user_id);
	$post = get_post($post_id);
	$post_author_data = get_userdata($post->post_author);

	if ( (($user_id == $post_author_data->ID) && !($post->post_status == 'publish' &&  $author_data->user_level < 2))
	     || ($author_data->user_level > $post_author_data->user_level)
	     || ($author_data->user_level >= 10) ) {
		return true;
	} else {
		return false;
	}
}

/* returns true if $user_id can delete $post_id */
function user_can_delete_post($user_id, $post_id, $blog_id = 1) {
	// right now if one can edit, one can delete
	return user_can_edit_post($user_id, $post_id, $blog_id);
}

/* returns true if $user_id can set new posts' dates on $blog_id */
function user_can_set_post_date($user_id, $blog_id = 1, $category_id = 'None') {
	$author_data = get_userdata($user_id);
	return (($author_data->user_level > 4) && user_can_create_post($user_id, $blog_id, $category_id));
}

/* returns true if $user_id can edit $post_id's date */
function user_can_edit_post_date($user_id, $post_id, $blog_id = 1) {
	$author_data = get_userdata($user_id);
	return (($author_data->user_level > 4) && user_can_edit_post($user_id, $post_id, $blog_id));
}

/* returns true if $user_id can edit $post_id's comments */
function user_can_edit_post_comments($user_id, $post_id, $blog_id = 1) {
	// right now if one can edit a post, one can edit comments made on it
	return user_can_edit_post($user_id, $post_id, $blog_id);
}

/* returns true if $user_id can delete $post_id's comments */
function user_can_delete_post_comments($user_id, $post_id, $blog_id = 1) {
	// right now if one can edit comments, one can delete comments
	return user_can_edit_post_comments($user_id, $post_id, $blog_id);
}

function user_can_edit_user($user_id, $other_user) {
	$user  = get_userdata($user_id);
	$other = get_userdata($other_user);
	if ( $user->user_level > $other->user_level || $user->user_level > 8 || $user->ID == $other->ID )
		return true;
	else
		return false;
}

function wp_blacklist_check($author, $email, $url, $comment, $user_ip, $user_agent) {
	global $wpdb;

	do_action('wp_blacklist_check', $author, $email, $url, $comment, $user_ip, $user_agent);

	if ( preg_match_all('/&#(\d+);/', $comment . $author . $url, $chars) ) {
		foreach ($chars[1] as $char) {
			// If it's an encoded char in the normal ASCII set, reject
			if ($char < 128)
				return true;
		}
	}

	$mod_keys = trim( get_settings('blacklist_keys') );
	if ('' == $mod_keys )
		return false; // If moderation keys are empty
	$words = explode("\n", $mod_keys );

	foreach ($words as $word) {
		$word = trim($word);

		// Skip empty lines
		if ( empty($word) ) { continue; }

		// Do some escaping magic so that '#' chars in the 
		// spam words don't break things:
		$word = preg_quote($word, '#');
		
		$pattern = "#$word#i"; 
		if ( preg_match($pattern, $author    ) ) return true;
		if ( preg_match($pattern, $email     ) ) return true;
		if ( preg_match($pattern, $url       ) ) return true;
		if ( preg_match($pattern, $comment   ) ) return true;
		if ( preg_match($pattern, $user_ip   ) ) return true;
		if ( preg_match($pattern, $user_agent) ) return true;
	}
	
	if ( isset($_SERVER['REMOTE_ADDR']) ) {
		if ( wp_proxy_check($_SERVER['REMOTE_ADDR']) ) return true;
	}

	return false;
}

function wp_proxy_check($ipnum) {
	if ( get_option('open_proxy_check') && isset($ipnum) ) {
		$rev_ip = implode( '.', array_reverse( explode( '.', $ipnum ) ) );
		$lookup = $rev_ip . '.opm.blitzed.org';
		if ( $lookup != gethostbyname( $lookup ) )
			return true;
	}

	return false;
}

function wp_new_comment( $commentdata, $spam = false ) {
	global $wpdb;

	$commentdata = apply_filters('preprocess_comment', $commentdata);
	extract($commentdata);

	$comment_post_ID = (int) $comment_post_ID;

	$user_id = apply_filters('pre_user_id', $user_ID);
	$author  = apply_filters('pre_comment_author_name', $comment_author);
	$email   = apply_filters('pre_comment_author_email', $comment_author_email);
	$url     = apply_filters('pre_comment_author_url', $comment_author_url);
	$comment = apply_filters('pre_comment_content', $comment_content);
	$comment = apply_filters('post_comment_text', $comment); // Deprecated
	$comment = apply_filters('comment_content_presave', $comment); // Deprecated

	$user_ip     = apply_filters('pre_comment_user_ip', $_SERVER['REMOTE_ADDR']);
	$user_domain = apply_filters('pre_comment_user_domain', gethostbyaddr($user_ip) );
	$user_agent  = apply_filters('pre_comment_user_agent', $_SERVER['HTTP_USER_AGENT']);

	$now     = current_time('mysql');
	$now_gmt = current_time('mysql', 1);

	if ( $user_id ) {
		$userdata = get_userdata($user_id);
		$post_author = $wpdb->get_var("SELECT post_author FROM $wpdb->posts WHERE ID = '$comment_post_ID' LIMIT 1");
	}

	// Simple duplicate check
	$dupe = "SELECT comment_ID FROM $wpdb->comments WHERE comment_post_ID = '$comment_post_ID' AND ( comment_author = '$author' ";
	if ( $email ) $dupe .= "OR comment_author_email = '$email' ";
	$dupe .= ") AND comment_content = '$comment' LIMIT 1";
	if ( $wpdb->get_var($dupe) )
		die( __('Duplicate comment detected; it looks as though you\'ve already said that!') );

	// Simple flood-protection
	if ( $lasttime = $wpdb->get_var("SELECT comment_date_gmt FROM $wpdb->comments WHERE comment_author_IP = '$user_ip' OR comment_author_email = '$email' ORDER BY comment_date DESC LIMIT 1") ) {
		$time_lastcomment = mysql2date('U', $lasttime);
		$time_newcomment  = mysql2date('U', $now_gmt);
		if ( ($time_newcomment - $time_lastcomment) < 15 ) {
			do_action('comment_flood_trigger', $time_lastcomment, $time_newcomment);
			die( __('Sorry, you can only post a new comment once every 15 seconds. Slow down cowboy.') );
		}
	}

	if ( $userdata && ( $user_id == $post_author || $userdata->user_level >= 9 ) ) {
		$approved = 1;
	} else {
		if ( check_comment($author, $email, $url, $comment, $user_ip, $user_agent, $comment_type) )
			$approved = 1;
		else
			$approved = 0;
		if ( wp_blacklist_check($author, $email, $url, $comment, $user_ip, $user_agent) )
			$approved = 'spam';
	}

	$approved = apply_filters('pre_comment_approved', $approved);

	$result = $wpdb->query("INSERT INTO $wpdb->comments 
	(comment_post_ID, comment_author, comment_author_email, comment_author_url, comment_author_IP, comment_date, comment_date_gmt, comment_content, comment_approved, comment_agent, comment_type, user_id)
	VALUES 
	('$comment_post_ID', '$author', '$email', '$url', '$user_ip', '$now', '$now_gmt', '$comment', '$approved', '$user_agent', '$comment_type', '$user_id')
	");

	$comment_id = $wpdb->insert_id;
	do_action('comment_post', $comment_id, $approved);

	if ( 'spam' !== $approved ) { // If it's spam save it silently for later crunching
		if ( '0' == $approved )
			wp_notify_moderator($comment_id);
	
		if ( get_settings('comments_notify') && $approved )
			wp_notify_postauthor($comment_id, $comment_type);
	}

	return $result;
}

function do_trackbacks($post_id) {
	global $wpdb;

	$post = $wpdb->get_row("SELECT * FROM $wpdb->posts WHERE ID = $post_id");
	$to_ping = get_to_ping($post_id);
	$pinged  = get_pung($post_id);
	if ( empty($to_ping) )
		return;
	if (empty($post->post_excerpt))
		$excerpt = apply_filters('the_content', $post->post_content);
	else
		$excerpt = apply_filters('the_excerpt', $post->post_excerpt);
	$excerpt = str_replace(']]>', ']]&gt;', $excerpt);
	$excerpt = strip_tags($excerpt);
	$excerpt = substr($excerpt, 0, 252) . '...';

	$post_title = apply_filters('the_title', $post->post_title);
	$post_title = strip_tags($post_title);

	if ($to_ping) : foreach ($to_ping as $tb_ping) :
		$tb_ping = trim($tb_ping);
		if ( !in_array($tb_ping, $pinged) )
		 trackback($tb_ping, $post_title, $excerpt, $post_id);
	endforeach; endif;
}

function get_pung($post_id) { // Get URIs already pung for a post
	global $wpdb;
	$pung = $wpdb->get_var("SELECT pinged FROM $wpdb->posts WHERE ID = $post_id");
	$pung = trim($pung);
	$pung = preg_split('/\s/', $pung);
	return $pung;
}

function get_enclosed($post_id) { // Get enclosures already enclosed for a post
	global $wpdb;
	$custom_fields = get_post_custom( $post_id );
	$pung = array();
	if( is_array( $custom_fields ) ) {
		while( list( $key, $val ) = each( $custom_fields ) ) { 
			if( $key == 'enclosure' ) {
				if (is_array($val)) {
					foreach($val as $enc) {
						$enclosure = split( "\n", $enc );
						$pung[] = trim( $enclosure[ 0 ] );
					}
				}
			}
		}
	}
	return $pung;
}

function get_to_ping($post_id) { // Get any URIs in the todo list
	global $wpdb;
	$to_ping = $wpdb->get_var("SELECT to_ping FROM $wpdb->posts WHERE ID = $post_id");
	$to_ping = trim($to_ping);
	$to_ping = preg_split('/\s/', $to_ping, -1, PREG_SPLIT_NO_EMPTY);
	return $to_ping;
}

function add_ping($post_id, $uri) { // Add a URI to those already pung
	global $wpdb;
	$pung = $wpdb->get_var("SELECT pinged FROM $wpdb->posts WHERE ID = $post_id");
	$pung = trim($pung);
	$pung = preg_split('/\s/', $pung);
	$pung[] = $uri;
	$new = implode("\n", $pung);
	return $wpdb->query("UPDATE $wpdb->posts SET pinged = '$new' WHERE ID = $post_id");
}

?>
