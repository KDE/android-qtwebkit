<?php
require_once('admin.php');
$title = __('Pages');
$parent_file = 'edit.php';
require_once('admin-header.php');

get_currentuserinfo();
?>

<div class="wrap">
<h2><?php _e('Page Management'); ?></h2>

<?php
if (isset($user_ID) && ('' != intval($user_ID))) {
	$posts = $wpdb->get_results("
	SELECT $wpdb->posts.*, $wpdb->users.user_level FROM $wpdb->posts
	INNER JOIN $wpdb->users ON ($wpdb->posts.post_author = $wpdb->users.ID)
	WHERE $wpdb->posts.post_status = 'static'
	AND ($wpdb->users.user_level < $user_level OR $wpdb->posts.post_author = $user_ID)
	");
} else {
    $posts = $wpdb->get_results("SELECT * FROM $wpdb->posts WHERE post_status = 'static'");
}

if ($posts) {
?>
<table width="100%" cellpadding="3" cellspacing="3"> 
  <tr> 
    <th scope="col"><?php _e('ID') ?></th> 
    <th scope="col"><?php _e('Title') ?></th> 
    <th scope="col"><?php _e('Owner') ?></th>
	<th scope="col"><?php _e('Updated') ?></th>
	<th scope="col"></th> 
    <th scope="col"></th> 
    <th scope="col"></th> 
  </tr> 
<?php page_rows(); ?>
</table> 
<?php
} else {
?>
<p><?php _e('No pages yet.') ?></p>
<?php
} // end if ($posts)
?> 
<p><?php _e('Pages are like posts except they live outside of the normal blog chronology. You can use pages to organize and manage any amount of content.'); ?></p>
<h3><a href="page-new.php"><?php _e('Create New Page'); ?> &raquo;</a></h3>
</div> 


<?php include('admin-footer.php'); ?> 
