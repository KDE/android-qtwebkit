<?php
require_once('admin.php');

$title = __('Writing Options');
$parent_file = 'options-general.php';

include('admin-header.php');
?>

<div class="wrap"> 
  <h2><?php _e('Writing Options') ?></h2> 
  <form name="form1" method="post" action="options.php"> 
    <input type="hidden" name="action" value="update" /> 
    <input type="hidden" name="page_options" value="'default_post_edit_rows','use_smilies','use_balanceTags','advanced_edit','ping_sites','mailserver_url', 'mailserver_port','mailserver_login','mailserver_pass','default_category','default_email_category','new_users_can_blog'" /> 
    <table width="100%" cellspacing="2" cellpadding="5" class="editform"> 
      <tr valign="top">
        <th scope="row"> <?php _e('When starting a post, show:') ?> </th>
        <td><?php get_settings('advanced_edit') ?><label>
          <input name="advanced_edit" type="radio" value="0" <?php checked('0', get_settings('advanced_edit')); ?> />
<?php _e('Simple controls') ?></label>
          <br />
          <label for="advanced_edit">
          <input name="advanced_edit" id="advanced_edit" type="radio" value="1" <?php checked('1', get_settings('advanced_edit')); ?> />
<?php _e('Advanced controls') ?></label>
		</td>
      </tr>
      <tr valign="top"> 
        <th width="33%" scope="row"> <?php _e('Size of the writing box:') ?></th> 
        <td><input name="default_post_edit_rows" type="text" id="default_post_edit_rows" value="<?php form_option('default_post_edit_rows'); ?>" size="2" style="width: 1.5em; " /> 
         <?php _e('lines') ?></td> 
      </tr> 
      <tr valign="top">
        <th scope="row"><?php _e('Formatting:') ?></th>
        <td>          <label for="label">
          <input name="use_smilies" type="checkbox" id="label" value="1" <?php checked('1', get_settings('use_smilies')); ?> />
          <?php _e('Convert emoticons like <code>:-)</code> and <code>:-P</code> to graphics on display') ?></label> <br />          <label for="label2">
  <input name="use_balanceTags" type="checkbox" id="label2" value="1" <?php checked('1', get_settings('use_balanceTags')); ?> />
          <?php _e('WordPress should correct invalidly nested XHTML automatically') ?></label></td>
      </tr>
        	<tr valign="top">
                <th scope="row"><?php _e('Default post category:') ?></th>
        		<td><select name="default_category" id="default_category">
<?php
$categories = $wpdb->get_results("SELECT * FROM $wpdb->categories ORDER BY cat_name");
foreach ($categories as $category) :
if ($category->cat_ID == get_settings('default_category')) $selected = " selected='selected'";
else $selected = '';
	echo "\n\t<option value='$category->cat_ID' $selected>$category->cat_name</option>";
endforeach;
?>
       			</select></td>
	</tr>
	<tr>
        <th scope="row"><?php _e('Newly registered members:') ?></th> 
        <td> <label for="new_users_can_blog0"><input name="new_users_can_blog" id="new_users_can_blog0" type="radio" value="0" <?php checked('0', get_settings('new_users_can_blog')); ?> /> <?php _e('Cannot write articles') ?></label><br />
<label for="new_users_can_blog1"><input name="new_users_can_blog" id="new_users_can_blog1" type="radio" value="1" <?php checked('1', get_settings('new_users_can_blog')); ?> /> <?php _e('May submit drafts for review') ?></label><br />
<label for="new_users_can_blog2"><input name="new_users_can_blog" id="new_users_can_blog2" type="radio" value="2" <?php checked('2', get_settings('new_users_can_blog')); ?> /> <?php _e('May publish articles') ?></label><br /></td> 
	</tr> 
</table>

<fieldset class="options">
	<legend><?php _e('Writing by e-mail') ?></legend>
	<p><?php printf(__('To post to WordPress by e-mail you must set up a secret e-mail account with POP3 access. Any mail received at this address will be posted, so it&#8217;s a good idea to keep this address very secret. Here are three random strings you could use: <code>%s</code>, <code>%s</code>, <code>%s</code>.'), substr(md5(uniqid(microtime())),0,5), substr(md5(uniqid(microtime())),0,5), substr(md5(uniqid(microtime())),0,5)) ?></p>
	
	<table width="100%" cellspacing="2" cellpadding="5" class="editform">
		<tr valign="top">
			<th scope="row"><?php _e('Mail server:') ?></th>
			<td><input name="mailserver_url" type="text" id="mailserver_url" value="<?php form_option('mailserver_url'); ?>" size="40" />
			<label for="mailserver_port"><?php _e('Port:') ?></label> 
			<input name="mailserver_port" type="text" id="mailserver_port" value="<?php form_option('mailserver_port'); ?>" size="6" />
			</td>
		</tr>
		<tr valign="top">
			<th width="33%" scope="row"><?php _e('Login name:') ?></th>
			<td><input name="mailserver_login" type="text" id="mailserver_login" value="<?php form_option('mailserver_login'); ?>" size="40" /></td>
		</tr>
		<tr valign="top">
			<th scope="row"><?php _e('Password:') ?></th>
			<td>
				<input name="mailserver_pass" type="text" id="mailserver_pass" value="<?php form_option('mailserver_pass'); ?>" size="40" />
			</td>
		</tr>
		<tr valign="top">
			<th scope="row"><?php _e('Default post by mail category:') ?></th>
			<td><select name="default_email_category" id="default_email_category">
<?php
//Alreay have $categories from default_category
foreach ($categories as $category) :
if ($category->cat_ID == get_settings('default_email_category')) $selected = " selected='selected'";
else $selected = '';
echo "\n\t<option value='$category->cat_ID' $selected>$category->cat_name</option>";
endforeach;
?>
			</select></td>
		</tr>
	</table>
</fieldset>

<fieldset class="options">
	<legend><?php _e('Update Services') ?></legend>
          <p><?php _e('When you publish a new post, WordPress automatically notifies the following site update services. For more about this, see <a href="http://codex.wordpress.org/Update_Services">Update Services</a> on the Codex. Separate multiple service URIs with line breaks.') ?></p>
	
	<textarea name="ping_sites" id="ping_sites" style="width: 98%;" rows="3" cols="50"><?php form_option('ping_sites'); ?></textarea>
</fieldset>

<p class="submit"> 
	<input type="submit" name="Submit" value="<?php _e('Update Options') ?> &raquo;" /> 
</p>
</form> 
</div> 

<?php include('./admin-footer.php') ?>