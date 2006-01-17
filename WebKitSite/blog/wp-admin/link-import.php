<?php
// Links
// Copyright (C) 2002 Mike Little -- mike@zed1.com

require_once('admin.php');
$parent_file = 'link-manager.php';
$title = __('Import Blogroll');
$this_file = 'link-import.php';

$step = $_POST['step'];
if (!$step) $step = 0;
?>
<?php
switch ($step) {
    case 0:
    {
        include_once('admin-header.php');
        if ($user_level < 5)
            die (__("Cheatin&#8217; uh?"));

        $opmltype = 'blogrolling'; // default.
?>

<div class="wrap">

    <h2><?php _e('Import your blogroll from another system') ?> </h2>
	<!-- <form name="blogroll" action="link-import.php" method="get"> -->
	<form enctype="multipart/form-data" action="link-import.php" method="post" name="blogroll">

	<ol>
    <li><?php _e('Go to <a href="http://www.blogrolling.com">Blogrolling.com</a> and sign in. Once you&#8217;ve done that, click on <strong>Get Code</strong>, and then look for the <strong><abbr title="Outline Processor Markup Language">OPML</abbr> code</strong>') ?>.</li>
    <li><?php _e('Or go to <a href="http://blo.gs">Blo.gs</a> and sign in. Once you&#8217;ve done that in the \'Welcome Back\' box on the right, click on <strong>share</strong>, and then look for the <strong><abbr title="Outline Processor Markup Language">OPML</abbr> link</strong> (favorites.opml).') ?></li>
    <li><?php _e('Select that text and copy it or copy the link/shortcut into the box below.') ?><br />
       <input type="hidden" name="step" value="1" />
       <?php _e('Your OPML URL:') ?> <input type="text" name="opml_url" size="65" />
	</li>
    <li>
	   <?php _e('<strong>or</strong> you can upload an OPML file from your desktop aggregator:') ?><br />
       <input type="hidden" name="MAX_FILE_SIZE" value="30000" />
       <label><?php _e('Upload this file:') ?> <input name="userfile" type="file" /></label>
    </li>

    <li><?php _e('Now select a category you want to put these links in.') ?><br />
	<?php _e('Category:') ?> <select name="cat_id">
<?php
	$categories = $wpdb->get_results("SELECT cat_id, cat_name, auto_toggle FROM $wpdb->linkcategories ORDER BY cat_id");
	foreach ($categories as $category) {
?>
    <option value="<?php echo $category->cat_id; ?>"><?php echo $category->cat_id.': '.$category->cat_name; ?></option>
<?php
        } // end foreach
?>
    </select>

	</li>

    <li><input type="submit" name="submit" value="<?php _e('Import!') ?>" /></li>
	</ol>
    </form>

</div>
<?php
                break;
            } // end case 0

    case 1: {
                include_once('admin-header.php');
                if ($user_level < 5)
                    die (__("Cheatin' uh ?"));
?>
<div class="wrap">

     <h2><?php _e('Importing...') ?></h2>
<?php
                $cat_id = $_POST['cat_id'];
                if (($cat_id == '') || ($cat_id == 0)) {
                    $cat_id  = 1;
                }

                $opml_url = $_POST['opml_url'];
                if (isset($opml_url) && $opml_url != '') {
					$blogrolling = true;
                }
                else // try to get the upload file.
				{
					$uploaddir = get_settings('fileupload_realpath');
					$uploadfile = $uploaddir.'/'.$_FILES['userfile']['name'];

					if (move_uploaded_file($_FILES['userfile']['tmp_name'], $uploadfile))
					{
						//echo "Upload successful.";
						$blogrolling = false;
						$opml_url = $uploadfile;
					} else {
						echo __("Upload error");
					}
				}

                if (isset($opml_url) && $opml_url != '') {
                    $opml = implode('', file($opml_url));
                    include_once('link-parse-opml.php');

                    $link_count = count($names);
                    for ($i = 0; $i < $link_count; $i++) {
                        if ('Last' == substr($titles[$i], 0, 4))
                            $titles[$i] = '';
                        if ('http' == substr($titles[$i], 0, 4))
                            $titles[$i] = '';
                        $query = "INSERT INTO $wpdb->links (link_url, link_name, link_target, link_category, link_description, link_owner, link_rss)
                                VALUES('{$urls[$i]}', '".addslashes($names[$i])."', '', $cat_id, '".addslashes($descriptions[$i])."', $user_ID, '{$feeds[$i]}')\n";
                        $result = $wpdb->query($query);
                        echo sprintf(__("<p>Inserted <strong>%s</strong></p>"), $names[$i]);
                    }
?>
     <p><?php printf(__('Inserted %1$d links into category %2$s. All done! Go <a href="%3$s">manage those links</a>.'), $link_count, $cat_id, 'link-manager.php') ?></p>
<?php
                } // end if got url
                else
                {
                    echo "<p>" . __("You need to supply your OPML url. Press back on your browser and try again") . "</p>\n";
                } // end else

?>
</div>
<?php
                break;
            } // end case 1
} // end switch
?>
</body>
</html>
