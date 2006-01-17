<?php

function get_the_password_form() {
    $output = '<form action="' . get_settings('siteurl') . '/wp-pass.php" method="post">
    <p>' . __("This post is password protected. To view it please enter your password below:") . '</p>
    <p><label>' . __("Password:") . ' <input name="post_password" type="password" size="20" /></label> <input type="submit" name="Submit" value="Submit" /></p>
    </form>
    ';
	return $output;
}

function the_ID() {
	global $id;
	echo $id;
}

function the_title($before = '', $after = '', $echo = true) {
	$title = get_the_title();
	if ( strlen($title) > 0 ) {
		$title = apply_filters('the_title', $before . $title . $after, $before, $after);
		if ($echo)
			echo $title;
		else
			return $title;
	}
}

function get_the_title($id = 0) {
	$post = &get_post($id);

	$title = $post->post_title;
	if (!empty($post->post_password))
		$title = sprintf(__('Protected: %s'), $title);

	return $title;
}

function get_the_guid( $id = 0 ) {
	$post = &get_post($id);
	
	return apply_filters('get_the_guid', $post->guid);
}

function the_guid( $id = 0 ) {
	echo get_the_guid($id);
}


function the_content($more_link_text = '(more...)', $stripteaser = 0, $more_file = '') {
    $content = get_the_content($more_link_text, $stripteaser, $more_file);
    $content = apply_filters('the_content', $content);
    $content = str_replace(']]>', ']]&gt;', $content);
    echo $content;
}

function get_the_content($more_link_text = '(more...)', $stripteaser = 0, $more_file = '') {
    global $id, $post, $more, $single, $withcomments, $page, $pages, $multipage, $numpages;
    global $preview;
    global $pagenow;
    $output = '';

    if (!empty($post->post_password)) { // if there's a password
        if (stripslashes($_COOKIE['wp-postpass_'.COOKIEHASH]) != $post->post_password) {  // and it doesn't match the cookie
            $output = get_the_password_form();
            return $output;
        }
    }

    if ($more_file != '') {
        $file = $more_file;
    } else {
        $file = $pagenow; //$_SERVER['PHP_SELF'];
    }
    $content = $pages[$page-1];
    $content = explode('<!--more-->', $content, 2);
    if ((preg_match('/<!--noteaser-->/', $post->post_content) && ((!$multipage) || ($page==1))))
        $stripteaser = 1;
    $teaser = $content[0];
    if (($more) && ($stripteaser))
        $teaser = '';
    $output .= $teaser;
    if (count($content)>1) {
        if ($more) {
            $output .= '<a id="more-'.$id.'"></a>'.$content[1];
        } else {
            $output .= ' <a href="'. get_permalink() . "#more-$id\">$more_link_text</a>";
        }
    }
    if ($preview) { // preview fix for javascript bug with foreign languages
        $output =  preg_replace('/\%u([0-9A-F]{4,4})/e',  "'&#'.base_convert('\\1',16,10).';'", $output);
    }
    return $output;
}

function the_excerpt() {
	echo apply_filters('the_excerpt', get_the_excerpt());
}

function get_the_excerpt($fakeit = true) {
	global $id, $post;
	$output = '';
	$output = $post->post_excerpt;
	if (!empty($post->post_password)) { // if there's a password
		if ($_COOKIE['wp-postpass_'.COOKIEHASH] != $post->post_password) {  // and it doesn't match the cookie
			$output = __('There is no excerpt because this is a protected post.');
			return $output;
		}
	}

	return apply_filters('get_the_excerpt', $output);
}

function wp_link_pages($args = '') {
	parse_str($args, $r);
	if (!isset($r['before'])) $r['before'] = '<p>' . __('Pages:');
	if (!isset($r['after'])) $r['after'] = '</p>';
	if (!isset($r['next_or_number'])) $r['next_or_number'] = 'number';
	if (!isset($r['nextpagelink'])) $r['nextpagelink'] = 'Next page';
	if (!isset($r['previouspagelink'])) $r['previouspagelink'] = 'Previous page';
	if (!isset($r['pagelink'])) $r['pagelink'] = '%';
	if (!isset($r['more_file'])) $r['more_file'] = '';
	link_pages($r['before'], $r['after'], $r['next_or_number'], $r['nextpagelink'], $r['previouspagelink'], $r['pagelink'], $r['more_file']);
}

function link_pages($before='<br />', $after='<br />', $next_or_number='number', $nextpagelink='next page', $previouspagelink='previous page', $pagelink='%', $more_file='') {
    global $id, $page, $numpages, $multipage, $more;
    global $pagenow;
    if ($more_file != '') {
        $file = $more_file;
    } else {
        $file = $pagenow;
    }
    if (($multipage)) {
        if ($next_or_number=='number') {
            echo $before;
            for ($i = 1; $i < ($numpages+1); $i = $i + 1) {
                $j=str_replace('%',"$i",$pagelink);
                echo ' ';
                if (($i != $page) || ((!$more) && ($page==1))) {
                    if ('' == get_settings('permalink_structure')) {
                        echo '<a href="' . get_permalink() . '&amp;page=' . $i . '">';
                    } else {
                        echo '<a href="' . trailingslashit( get_permalink() ) . $i . '/">';
                    }
                }
                echo $j;
                if (($i != $page) || ((!$more) && ($page==1)))
                    echo '</a>';
            }
            echo $after;
        } else {
            if ($more) {
                echo $before;
                $i=$page-1;
                if ($i && $more) {
                    if ('' == get_settings('permalink_structure')) {
                        echo '<a href="' . get_permalink() . '&amp;page=' . $i . '">'.$previouspagelink.'</a>';
                    } else {
                        echo '<a href="' . get_permalink() . $i . '/">'.$previouspagelink.'</a>';
                    }
                }
                $i=$page+1;
                if ($i<=$numpages && $more) {
                    if ('' == get_settings('permalink_structure')) {
                        echo '<a href="'.get_permalink() . '&amp;page=' . $i . '">'.$nextpagelink.'</a>';
                    } else {
                        echo '<a href="'.get_permalink().$i.'/">'.$nextpagelink.'</a>';
                    }
                }
                echo $after;
            }
        }
    }
}

/*
 * Post-meta: Custom per-post fields.
 */
 
function get_post_custom( $post_id = 0 ) {
	global $id, $post_meta_cache, $wpdb;
	if ( $post_id )
		$id = $post_id;
	if ( isset($post_meta_cache[$id]) ) {
		return $post_meta_cache[$id];
	} else {
	if ( $meta_list = $wpdb->get_results("SELECT post_id, meta_key, meta_value FROM $wpdb->postmeta  WHERE post_id = '$id' ORDER BY post_id, meta_key", ARRAY_A) ) {
		
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
	return $post_meta_cache[$mpid];
    }
	}
}

function get_post_custom_keys() {
	global $id, $post_meta_cache;
	
	if (!is_array($post_meta_cache[$id]))
		return;
	if ($keys = array_keys($post_meta_cache[$id]))
		return $keys;
}

function get_post_custom_values($key='') {
	global $id, $post_meta_cache;

	return $post_meta_cache[$id][$key];
}

function post_custom( $key = '' ) {
	global $id, $post_meta_cache;
	
	if ( 1 == count($post_meta_cache[$id][$key]) ) return $post_meta_cache[$id][$key][0];
	else return $post_meta_cache[$id][$key];
}

// this will probably change at some point...
function the_meta() {
	global $id, $post_meta_cache;
	
	if ($keys = get_post_custom_keys()) {
		echo "<ul class='post-meta'>\n";
		foreach ($keys as $key) {
			$values = array_map('trim',$post_meta_cache[$id][$key]);
			$value = implode($values,', ');
			
			echo "<li><span class='post-meta-key'>$key:</span> $value</li>\n";
		}
		echo "</ul>\n";
	}
}


//
// Pages
//

function &get_page_children($page_id, $pages) {
	global $page_cache;

	if ( empty($pages) )
		$pages = &$page_cache;

	$page_list = array();
	foreach ($pages as $page) {
		if ($page->post_parent == $page_id) {
			$page_list[] = $page;
			if ( $children = get_page_children($page->ID, $pages)) {
				$page_list = array_merge($page_list, $children);
			}
		}
	}

	return $page_list;
}

function &get_pages($args = '') {
	global $wpdb;

	parse_str($args, $r);

	if (!isset($r['child_of'])) $r['child_of'] = 0;
	if (!isset($r['sort_column'])) $r['sort_column'] = 'post_title';
	if (!isset($r['sort_order'])) $r['sort_order'] = 'ASC';

	$exclusions = '';
	if (!empty($r['exclude'])) {
		$expages = preg_split('/[\s,]+/',$r['exclude']);
		if (count($expages)) {
			foreach ($expages as $expage) {
				$exclusions .= ' AND ID <> ' . intval($expage) . ' ';
			}
		}
	}

	$pages = $wpdb->get_results("SELECT * " .
															"FROM $wpdb->posts " .
															"WHERE post_status = 'static' " .
															"$exclusions " .
															"ORDER BY " . $r['sort_column'] . " " . $r['sort_order']);

	if ( empty($pages) )
		return array();

	// Update cache.
	update_page_cache($pages);

	if ($r['child_of'])
		$pages = & get_page_children($r['child_of'], $pages);

	return $pages;
}

function wp_list_pages($args = '') {
	parse_str($args, $r);
	if ( !isset($r['depth']) ) $r['depth'] = 0;
	if ( !isset($r['show_date']) ) $r['show_date'] = '';
	if ( !isset($r['child_of']) ) $r['child_of'] = 0;
	if ( !isset($r['title_li']) ) $r['title_li'] = __('Pages');
	if ( !isset($r['echo']) ) $r['echo'] = 1;
	
	$output = '';

	// Query pages.
	$pages = & get_pages($args);
	if ( $pages ) :

	if ( $r['title_li'] )
		$output .= '<li class="pagenav">' . $r['title_li'] . '<ul>';
	// Now loop over all pages that were selected
	$page_tree = Array();
	foreach($pages as $page) {
		// set the title for the current page
		$page_tree[$page->ID]['title'] = $page->post_title;
		$page_tree[$page->ID]['name'] = $page->post_name;

		// set the selected date for the current page
		// depending on the query arguments this is either
		// the createtion date or the modification date
		// as a unix timestamp. It will also always be in the
		// ts field.
		if (! empty($r['show_date'])) {
			if ('modified' == $r['show_date'])
				$page_tree[$page->ID]['ts'] = $page->post_modified;
			else
				$page_tree[$page->ID]['ts'] = $page->post_date;
		}

		// The tricky bit!!
		// Using the parent ID of the current page as the
		// array index we set the curent page as a child of that page.
		// We can now start looping over the $page_tree array
		// with any ID which will output the page links from that ID downwards.
		if ( $page->post_parent != $page->ID)
			$page_tree[$page->post_parent]['children'][] = $page->ID;
	}
	// Output of the pages starting with child_of as the root ID.
	// child_of defaults to 0 if not supplied in the query.
	$output .= _page_level_out($r['child_of'],$page_tree, $r, 0, false);
	if ( $r['title_li'] )
		$output .= '</ul></li>';
	endif;
	
	$output = apply_filters('wp_list_pages', $output);
	
	if ( $r['echo'] )
		echo $output;
	else 
		return $output;
}

function _page_level_out($parent, $page_tree, $args, $depth = 0, $echo = true) {
	global $wp_query;

	$queried_obj = $wp_query->get_queried_object();

	$output = '';

	if($depth)
		$indent = str_repeat("\t", $depth);
	//$indent = join('', array_fill(0,$depth,"\t"));

	foreach($page_tree[$parent]['children'] as $page_id) {
		$cur_page = $page_tree[$page_id];
		$title = $cur_page['title'];

		$css_class = 'page_item';
		if( $page_id == $queried_obj->ID) {
			$css_class .= ' current_page_item';
		}

		$output .= $indent . '<li class="' . $css_class . '"><a href="' . get_page_link($page_id) . '" title="' . wp_specialchars($title) . '">' . $title . '</a>';

		if(isset($cur_page['ts'])) {
			$format = get_settings('date_format');
			if(isset($args['date_format']))
				$format = $args['date_format'];
			$output .= " " . mysql2date($format, $cur_page['ts']);
		}
		echo "\n";

		if(isset($cur_page['children']) && is_array($cur_page['children'])) {
			$new_depth = $depth + 1;

			if(!$args['depth'] || $depth < ($args['depth']-1)) {
				$output .= "$indent<ul>\n";
				$output .= _page_level_out($page_id, $page_tree, $args, $new_depth, false);
				$output .= "$indent</ul>\n";
			}
		}
		$output .= "$indent</li>\n";
	}
	if ( $echo )
		echo $output;
	else
		return $output;
}

?>
