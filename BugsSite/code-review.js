// Copyright (C) 2010 Adam Barth. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

(function() {
  /**
   * Create a new function with some of its arguements
   * pre-filled.
   * Taken from goog.partial in the Closure library.
   * @param {Function} fn A function to partially apply.
   * @param {...*} var_args Additional arguments that are partially
   *     applied to fn.
   * @return {!Function} A partially-applied form of the function.
   */
  function partial(fn, var_args) {
    var args = Array.prototype.slice.call(arguments, 1);
    return function() {
      // Prepend the bound arguments to the current arguments.
      var newArgs = Array.prototype.slice.call(arguments);
      newArgs.unshift.apply(newArgs, args);
      return fn.apply(this, newArgs);
    };
  };

  function determineAttachmentID() {
    try {
      return /id=(\d+)/.exec(window.location.search)[1]
    } catch (ex) {
      return;
    }
  }

  // Attempt to activate only in the "Review Patch" context.
  if (window.top != window)
    return;
  if (!window.location.search.match(/action=review/))
    return;
  var attachment_id = determineAttachmentID();
  if (!attachment_id)
    return;

  var next_line_id = 0;
  var files = {};
  var original_file_contents = {};
  var patched_file_contents = {};
  var WEBKIT_BASE_DIR = "http://svn.webkit.org/repository/webkit/trunk/";

  function idForLine(number) {
    return 'line' + number;
  }

  function nextLineID() {
    return idForLine(next_line_id++);
  }

  function forEachLine(callback) {
    for (var i = 0; i < next_line_id; ++i) {
      callback($('#' + idForLine(i)));
    }
  }

  function idify() {
    this.id = nextLineID();
  }

  function hoverify() {
    $(this).hover(function() {
      $(this).addClass('hot');
    },
    function () {
      $(this).removeClass('hot');
    });
  }

  function previousCommentsFor(line) {
    var comments = [];
    var position = line;
    while (position.next() && position.next().hasClass('previousComment')) {
      position = position.next();
      comments.push(position.get());
    }
    return $(comments);
  }

  function findCommentPositionFor(line) {
    var position = line;
    while (position.next() && position.next().hasClass('previousComment'))
      position = position.next();
    return position;
  }

  function findCommentBlockFor(line) {
    var comment_block = findCommentPositionFor(line).next();
    if (!comment_block.hasClass('comment'))
      return;
    return comment_block;
  }

  function insertCommentFor(line, block) {
    findCommentPositionFor(line).after(block);
  }

  function addCommentFor(line) {
    if (line.attr('data-has-comment')) {
      // FIXME: This query is overly complex because we place comment blocks
      // after Lines.  Instead, comment blocks should be children of Lines.
      findCommentPositionFor(line).next().next().filter('.frozenComment').each(unfreezeComment);
      return;
    }
    line.attr('data-has-comment', 'true');
    line.addClass('commentContext');

    var comment_block = $('<div class="comment"><textarea data-comment-for="' + line.attr('id') + '"></textarea><div class="actions"><button class="ok">OK</button><button class="discard">Discard</button></div></div>');
    insertCommentFor(line, comment_block);
    comment_block.hide().slideDown('fast', function() {
      $(this).children('textarea').focus();
    });
  }

  function addCommentField() {
    var id = $(this).attr('data-comment-for');
    if (!id)
      id = this.id;
    addCommentFor($('#' + id));
  }

  function addPreviousComment(line, author, comment_text) {
    var comment_block = $('<div data-comment-for="' + line.attr('id') + '" class="previousComment"></div>');
    var author_block = $('<div class="author"></div>').text(author + ':');
    var text_block = $('<div class="content"></div>').text(comment_text);
    comment_block.append(author_block).append(text_block).each(hoverify).click(addCommentField);
    insertCommentFor(line, comment_block);
  }

  function displayPreviousComments(comments) {
    for (var i = 0; i < comments.length; ++i) {
      var author = comments[i].author;
      var file_name = comments[i].file_name;
      var line_number = comments[i].line_number;
      var comment_text = comments[i].comment_text;

      var file = files[file_name];

      var query = '.Line .to';
      if (line_number[0] == '-') {
        // The line_number represent a removal.  We need to adjust the query to
        // look at the "from" lines.
        query = '.Line .from';
        // Trim off the '-' control character.
        line_number = line_number.substr(1);
      }

      $(file).find(query).each(function() {
        if ($(this).text() != line_number)
          return;
        var line = $(this).parent();
        addPreviousComment(line, author, comment_text);
      });
    }
    if (comments.length == 0)
      return;
    descriptor = comments.length + ' comment';
    if (comments.length > 1)
      descriptor += 's';
    $('.message .commentStatus').text('This patch has ' + descriptor + '.  Scroll through them with the "n" and "p" keys.');
  }

  function scanForComments(author, text) {
    var comments = []
    var lines = text.split('\n');
    for (var i = 0; i < lines.length; ++i) {
      var parts = lines[i].match(/^([> ]+)([^:]+):(-?\d+)$/);
      if (!parts)
        continue;
      var quote_markers = parts[1];
      var file_name = parts[2];
      var line_number = parts[3];
      if (!file_name in files)
        continue;
      while (i < lines.length && lines[i].length > 0 && lines[i][0] == '>')
        ++i;
      var comment_lines = [];
      while (i < lines.length && (lines[i].length == 0 || lines[i][0] != '>')) {
        comment_lines.push(lines[i]);
        ++i;
      }
      --i; // Decrement i because the for loop will increment it again in a second.
      var comment_text = comment_lines.join('\n').trim();
      comments.push({
        'author': author,
        'file_name': file_name,
        'line_number': line_number,
        'comment_text': comment_text
      });
    }
    return comments;
  }

  function isReviewFlag(select) {
    return $(select).attr('title') == 'Request for patch review.';
  }

  function isCommitQueueFlag(select) {
    return $(select).attr('title').match(/commit-queue/);
  }

  function findControlForFlag(select) {
    if (isReviewFlag(select))
      return $('#toolbar .review select');
    else if (isCommitQueueFlag(select))
      return $('#toolbar .commitQueue select');
    return $();
  }

  function addFlagsForAttachment(details) {
    var flag_control = "<select><option></option><option>?</option><option>+</option><option>-</option></select>";
    $('#flagContainer').append(
      $('<span class="review"> r: ' + flag_control + '</span>')).append(
      $('<span class="commitQueue"> cq: ' + flag_control + '</span>'));

    details.find('#flags select').each(function() {
      var requestee = $(this).parent().siblings('td:first-child').text().trim();
      if (requestee.length) {
        // Remove trailing ':'.
        requestee = requestee.substr(0, requestee.length - 1);
        requestee = ' (' + requestee + ')';
      }
      var control = findControlForFlag(this)
      control.attr('selectedIndex', $(this).attr('selectedIndex'));
      control.parent().prepend(requestee);
    });
  }

  function fetchHistory() {
    $.get('attachment.cgi?id=' + attachment_id + '&action=edit', function(data) {
      var bug_id = /Attachment \d+ Details for Bug (\d+)/.exec(data)[1];
      $.get('show_bug.cgi?id=' + bug_id, function(data) {
        var comments = [];
        $(data).find('.bz_comment').each(function() {
          var author = $(this).find('.email').text();
          var text = $(this).find('.bz_comment_text').text();
          var comment_marker = '(From update of attachment ' + attachment_id + ' .details.)';
          if (text.match(comment_marker))
            $.merge(comments, scanForComments(author, text));
        });
        displayPreviousComments(comments);
      });

      var details = $(data);
      addFlagsForAttachment(details);
      $('#statusBubbleContainer').append($('<iframe style="margin-top:2px;" class="statusBubble" src="https://webkit-commit-queue.appspot.com/status-bubble/' + attachment_id + '" scrolling="no"></iframe>'));
      $('#toolbar .bugLink').html('<a href="/show_bug.cgi?id=' + bug_id + '" target="_blank">Bug ' + bug_id + '</a>');
    });
  }

  function crawlDiff() {
    $('.Line').each(idify).each(hoverify);
    $('.FileDiff').each(function() {
      var file_name = $(this).children('h1').text();
      files[file_name] = this;
      addExpandLinks(file_name);
    });
  }

  function addExpandLinks(file_name) {
    if (file_name.indexOf('ChangeLog') != -1)
      return;

    var file_diff = files[file_name];
    $('.context', file_diff).detach();

    var expand_bar_index = 0;

    // Don't show the links to expand upwards/downwards if the patch starts/ends without context
    // lines, i.e. starts/ends with add/remove lines.
    var first_line = file_diff.querySelector('.Line');
    if (!$(first_line).hasClass('add') && !$(first_line).hasClass('remove'))
      $('h1', file_diff).after(expandBarHtml(file_name, BELOW))

    $('br').replaceWith(expandBarHtml(file_name));

    var last_line = file_diff.querySelector('.Line:last-of-type');
    // Some patches for new files somehow end up with an empty context line at the end
    // with a from line number of 0. Don't show expand links in that case either.
    if (!$(last_line).hasClass('add') && !$(last_line).hasClass('remove') && fromLineNumber(last_line) != 0)
      $(file_diff).append(expandBarHtml(file_name, ABOVE));
  }

  function expandBarHtml(file_name, opt_direction) {
    var html = '<div class="ExpandBar">' +
        '<pre class="ExpandArea Expand' + ABOVE + '"></pre>' +
        '<div class="ExpandLinkContainer"><span class="ExpandText">expand: </span>';

    // FIXME: If there are <100 line to expand, don't show the expand-100 link.
    // If there are <20 lines to expand, don't show the expand-20 link.
    if (!opt_direction || opt_direction == ABOVE) {
      html += expandLinkHtml(ABOVE, 100) +
          expandLinkHtml(ABOVE, 20);
    }

    html += expandLinkHtml(ALL);

    if (!opt_direction || opt_direction == BELOW) {
      html += expandLinkHtml(BELOW, 20) +
        expandLinkHtml(BELOW, 100);
    }

    html += '</div><pre class="ExpandArea Expand' + BELOW + '"></pre></div>';
    return html;
  }

  function expandLinkHtml(direction, amount) {
    return "<a class='ExpandLink' href='javascript:' data-direction='" + direction + "' data-amount='" + amount + "'>" +
        (amount ? amount + " " : "") + direction + "</a>";
  }

  $(window).bind('click', function (e) {
    var target = e.target;
    if (target.className != 'ExpandLink')
      return;

    // Can't use $ here because something in the window's scope sets $ to something other than jQuery.
    var expand_bar = jQuery(target).parents('.ExpandBar');
    var file_name = expand_bar.parents('.FileDiff').children('h1')[0].textContent;
    var expand_function = partial(expand, expand_bar[0], file_name, target.getAttribute('data-direction'), Number(target.getAttribute('data-amount')));
    if (file_name in original_file_contents)
      expand_function();
    else
      getWebKitSourceFile(file_name, expand_function, expand_bar);
  });

  function getWebKitSourceFile(file_name, onLoad, expand_bar) {
    function handleLoad(contents) {
      original_file_contents[file_name] = contents.split('\n');
      patched_file_contents[file_name] = applyDiff(original_file_contents[file_name], file_name);
      onLoad();
    };

    $.ajax({
      url: WEBKIT_BASE_DIR + file_name,
      context: document.body,
      complete: function(xhr, data) {
              if (xhr.status == 0)
                  handleLoadError(expand_bar);
              else
                  handleLoad(xhr.responseText);
      }
    });
  }

  function replaceExpandLinkContainers(expand_bar, text) {
    $('.ExpandLinkContainer', $(expand_bar).parents('.FileDiff')).replaceWith('<span class="ExpandText">' + text + '</span>');
  }

  function handleLoadError(expand_bar) {
    // FIXME: In this case, try fetching the source file at the revision the patch was created at,
    // in case the file has bee deleted.
    // Might need to modify webkit-patch to include that data in the diff.
    replaceExpandLinkContainers(expand_bar, "Can't expand. Is this a new or deleted file?");
  }

  var ABOVE = 'above';
  var BELOW = 'below';
  var ALL = 'all';

  function expand(expand_bar, file_name, direction, amount) {
    if (file_name in original_file_contents && !patched_file_contents[file_name]) {
      // FIXME: In this case, try fetching the source file at the revision the patch was created at.
      // Might need to modify webkit-patch to include that data in the diff.
      replaceExpandLinkContainers(expand_bar, "Can't expand. Unable to apply patch to tip of tree.");
      return;
    }

    var above_expansion = expand_bar.querySelector('.Expand' + ABOVE)
    var below_expansion = expand_bar.querySelector('.Expand' + BELOW)

    var above_last_line = above_expansion.querySelector('.ExpansionLine:last-of-type');
    if (!above_last_line) {
      var diff_section = expand_bar.previousElementSibling;
      above_last_line = diff_section.querySelector('.Line:last-of-type');
    }

    var above_last_line_num, above_last_from_line_num;
    if (above_last_line) {
      above_last_line_num = toLineNumber(above_last_line);
      above_last_from_line_num = fromLineNumber(above_last_line);
    } else
      above_last_from_line_num = above_last_line_num = 0;

    var below_first_line = below_expansion.querySelector('.ExpansionLine');
    if (!below_first_line) {
      var diff_section = expand_bar.nextElementSibling;
      if (diff_section)
        below_first_line = diff_section.querySelector('.Line');
    }

    var below_first_line_num, below_first_from_line_num;
    if (below_first_line) {
      below_first_line_num = toLineNumber(below_first_line) - 1;
      below_first_from_line_num = fromLineNumber(below_first_line) - 1;
    } else
      below_first_from_line_num = below_first_line_num = patched_file_contents[file_name].length - 1;

    var start_line_num, start_from_line_num;
    var end_line_num;

    if (direction == ABOVE) {
      start_from_line_num = above_last_from_line_num;
      start_line_num = above_last_line_num;
      end_line_num = Math.min(start_line_num + amount, below_first_line_num);
    } else if (direction == BELOW) {
      end_line_num = below_first_line_num;
      start_line_num = Math.max(end_line_num - amount, above_last_line_num)
      start_from_line_num = Math.max(below_first_from_line_num - amount, above_last_from_line_num)
    } else { // direction == ALL
      start_line_num = above_last_line_num;
      start_from_line_num = above_last_from_line_num;
      end_line_num = below_first_line_num;
    }

    var expansion_area;
    // Filling in all the remaining lines. Overwrite the expand links.
    if (start_line_num == above_last_line_num && end_line_num == below_first_line_num) {
      expansion_area = expand_bar.querySelector('.ExpandLinkContainer');
      expansion_area.innerHTML = '';
    } else {
      expansion_area = direction == ABOVE ? above_expansion : below_expansion;
    }

    insertLines(file_name, expansion_area, direction, start_line_num, end_line_num, start_from_line_num);
  }

  function insertLines(file_name, expansion_area, direction, start_line_num, end_line_num, start_from_line_num) {
    var fragment = document.createDocumentFragment();
    for (var i = 0; i < end_line_num - start_line_num; i++) {
      var line = document.createElement('div');
      line.className = 'ExpansionLine';
      // FIXME: from line numbers are wrong
      line.innerHTML = '<span class="from expansionlineNumber">' + (start_from_line_num + i + 1) +
          '</span><span class="to expansionlineNumber">' + (start_line_num + i + 1) +
          '</span> <span class="text"></span>';
      line.querySelector('.text').textContent = patched_file_contents[file_name][start_line_num + i];
      fragment.appendChild(line);
    }

    if (direction == BELOW)
      expansion_area.insertBefore(fragment, expansion_area.firstChild);
    else
      expansion_area.appendChild(fragment);
  }

  function hunkStartingLine(patched_file, context, prev_line, hunk_num) {
    var PATCH_FUZZ = 2;
    var current_line = -1;
    var last_context_line = context[context.length - 1];
    if (patched_file[prev_line] == last_context_line)
      current_line = prev_line + 1;
    else {
      for (var i = prev_line - PATCH_FUZZ; i < prev_line + PATCH_FUZZ; i++) {
        if (patched_file[i] == last_context_line)
          current_line = i + 1;
      }

      if (current_line == -1) {
        console.log('Hunk #' + hunk_num + ' FAILED.');
        return -1;
      }
    }

    // For paranoia sake, confirm the rest of the context matches;
    for (var i = 0; i < context.length - 1; i++) {
      if (patched_file[current_line - context.length + i] != context[i]) {
        console.log('Hunk #' + hunk_num + ' FAILED. Did not match preceding context.');
        return -1;
      }
    }

    return current_line;
  }

  function fromLineNumber(line) {
    return Number(line.querySelector('.from').textContent);
  }

  function toLineNumber(line) {
    return Number(line.querySelector('.to').textContent);
  }

  function lineNumberForFirstNonContextLine(patched_file, line, prev_line, context, hunk_num) {
    if (context.length) {
      var prev_line_num = fromLineNumber(prev_line) - 1;
      return hunkStartingLine(patched_file, context, prev_line_num, hunk_num);
    }

    if (toLineNumber(line) == 1 || fromLineNumber(line) == 1)
      return 0;

    console.log('Failed to apply patch. Adds or removes lines before any context lines.');
    return -1;
  }

  function applyDiff(original_file, file_name) {
    var diff_sections = files[file_name].getElementsByClassName('DiffSection');
    var patched_file = original_file.concat([]);

    // Apply diffs in reverse order to avoid needing to keep track of changing line numbers.
    for (var i = diff_sections.length - 1; i >= 0; i--) {
      var section = diff_sections[i];
      var lines = section.getElementsByClassName('Line');
      var current_line = -1;
      var context = [];
      var hunk_num = i + 1;

      for (var j = 0, lines_len = lines.length; j < lines_len; j++) {
        var line = lines[j];
        var line_contents = line.querySelector('.text').textContent;
        if ($(line).hasClass('add')) {
          if (current_line == -1) {
            current_line = lineNumberForFirstNonContextLine(patched_file, line, lines[j-1], context, hunk_num);
            if (current_line == -1)
              return null;
          }

          patched_file.splice(current_line, 0, line_contents);
          current_line++;
        } else if ($(line).hasClass('remove')) {
          if (current_line == -1) {
            current_line = lineNumberForFirstNonContextLine(patched_file, line, lines[j-1], context, hunk_num);
            if (current_line == -1)
              return null;
          }

          if (patched_file[current_line] != line_contents) {
            console.log('Hunk #' + hunk_num + ' FAILED.');
            return null;
          }

          patched_file.splice(current_line, 1);
        } else if (current_line == -1) {
          context.push(line_contents);
        } else if (line_contents != patched_file[current_line]) {
          console.log('Hunk #' + hunk_num + ' FAILED. Context at end did not match');
          return null;
        } else {
          current_line++;
        }
      }
    }

    return patched_file;
  }

  function openOverallComments(e) {
    $('.overallComments textarea').addClass('open');
    $('#statusBubbleContainer').addClass('wrap');
  }

  $(document).ready(function() {
    crawlDiff();
    fetchHistory();
    $(document.body).prepend('<div id="message"><div class="help">Select line numbers to add a comment.</div><div class="commentStatus"></div></div>');
    $(document.body).prepend('<div id="toolbar">' +
        '<div class="overallComments">' +
            '<textarea placeholder="Overall comments"></textarea>' +
        '</div>' +
        '<div>' +
          '<span id="statusBubbleContainer"></span>' +
          '<span class="actions">' +
              '<span class="links"><span class="bugLink"></span></span>' +
              '<span id="flagContainer"></span>' +
              '<button id="preview_comments">Preview</button>' +
              '<button id="post_comments">Publish</button> ' +
          '</span></div>' +
        '</div>' +
        '</div>');

    $('.overallComments textarea').bind('click', openOverallComments);

    $(document.body).prepend('<div id="comment_form" class="inactive"><div class="winter"></div><div class="lightbox"><iframe id="reviewform" src="attachment.cgi?id=' + attachment_id + '&action=reviewform"></iframe></div></div>');
  });

  function discardComment() {
    var line_id = $(this).parentsUntil('.comment').parent().find('textarea').attr('data-comment-for');
    var line = $('#' + line_id)
    findCommentBlockFor(line).slideUp('fast', function() {
      $(this).remove();
      line.removeAttr('data-has-comment');
      trimCommentContextToBefore(line);
    });
  }

  function unfreezeComment() {
    $(this).prev().show();
    $(this).remove();
  }

  $('.comment .discard').live('click', discardComment);

  $('.comment .ok').live('click', function() {
    var comment_textarea = $(this).parentsUntil('.comment').parent().find('textarea');
    if (comment_textarea.val().trim() == '') {
      discardComment.call(this);
      return;
    }
    var line_id = comment_textarea.attr('data-comment-for');
    var line = $('#' + line_id)
    findCommentBlockFor(line).hide().after($('<div class="frozenComment"></div>').text(comment_textarea.val()));
  });

  $('.frozenComment').live('click', unfreezeComment);

  function focusOn(comment) {
    $('.focused').removeClass('focused');
    if (comment.length == 0)
      return;
    $(document).scrollTop(comment.addClass('focused').position().top - window.innerHeight/2);
  }

  function focusNextComment() {
    var comments = $('.previousComment');
    if (comments.length == 0)
      return;
    var index = comments.index($('.focused'));
    // Notice that -1 gets mapped to 0.
    focusOn($(comments.get(index + 1)));
  }

  function focusPreviousComment() {
    var comments = $('.previousComment');
    if (comments.length == 0)
      return;
    var index = comments.index($('.focused'));
    if (index == -1)
      index = comments.length;
    if (index == 0) {
      focusOn([]);
      return;
    }
    focusOn($(comments.get(index - 1)));
  }

  var kCharCodeForN = 'n'.charCodeAt(0);
  var kCharCodeForP = 'p'.charCodeAt(0);

  $('body').live('keypress', function() {
    // FIXME: There's got to be a better way to avoid seeing these keypress
    // events.
    if (event.target.nodeName == 'TEXTAREA')
      return;
    if (event.charCode == kCharCodeForN)
      focusNextComment();
    else if (event.charCode == kCharCodeForP)
      focusPreviousComment();
  });

  function contextLinesFor(line) {
    var context = [];
    while (line.hasClass('commentContext')) {
      $.merge(context, line);
      line = line.prev();
    }
    return $(context.reverse());
  }

  function trimCommentContextToBefore(line) {
    while (line.hasClass('commentContext') && line.attr('data-has-comment') != 'true') {
      line.removeClass('commentContext');
      line = line.prev();
    }
  }

  var in_drag_select = false;

  function stopDragSelect() {
    $('.selected').removeClass('selected');
    in_drag_select = false;
  }

  $('.lineNumber').live('click', function() {
    var line = $(this).parent();
    if (line.hasClass('commentContext'))
      trimCommentContextToBefore(line.prev());
  }).live('mousedown', function() {
    in_drag_select = true;
    $(this).parent().addClass('selected');
    event.preventDefault();
  });
  
  $('.Line').live('mouseenter', function() {
    if (!in_drag_select)
      return;

    var before = $(this).prevUntil('.selected')
    if (before.prev().hasClass('selected'))
      before.addClass('selected');

    var after = $(this).nextUntil('.selected')
    if (after.next().hasClass('selected'))
      after.addClass('selected');

    $(this).addClass('selected');
  }).live('mouseup', function() {
    if (!in_drag_select)
      return;
    var selected = $('.selected');
    var should_add_comment = !selected.last().next().hasClass('commentContext');
    selected.addClass('commentContext');
    if (should_add_comment)
      addCommentFor(selected.last());
  });

  $('.DiffSection').live('mouseleave', stopDragSelect).live('mouseup', stopDragSelect);

  function contextSnippetFor(line, indent) {
    var snippets = []
    contextLinesFor(line).each(function() {
      var action = ' ';
      if ($(this).hasClass('add'))
        action = '+';
      else if ($(this).hasClass('remove'))
        action = '-';
      var text = $(this).children('.text').text();
      snippets.push(indent + action + text);
    });
    return snippets.join('\n');
  }

  function fileNameFor(line) {
    return line.parentsUntil('.FileDiff').parent().find('h1').text();
  }

  function indentFor(depth) {
    return (new Array(depth + 1)).join('>') + ' ';
  }

  function snippetFor(line, indent) {
    var file_name = fileNameFor(line);
    var line_number = line.hasClass('remove') ? '-' + line.children('.from').text() : line.children('.to').text();
    return indent + file_name + ':' + line_number + '\n' + contextSnippetFor(line, indent);
  }

  function quotePreviousComments(comments) {
    var quoted_comments = [];
    var depth = comments.size();
    comments.each(function() {
      var indent = indentFor(depth--);
      var text = $(this).children('.content').text();
      quoted_comments.push(indent + '\n' + indent + text.split('\n').join('\n' + indent));
    });
    return quoted_comments.join('\n');
  }

  $('#comment_form .winter').live('click', function() {
    $('#comment_form').addClass('inactive');
  });

  function fillInReviewForm() {
    var comments_in_context = []
    forEachLine(function(line) {
      if (line.attr('data-has-comment') != 'true')
        return;
      var comment = findCommentBlockFor(line).children('textarea').val().trim();
      if (comment == '')
        return;
      var previous_comments = previousCommentsFor(line);
      var snippet = snippetFor(line, indentFor(previous_comments.size() + 1));
      var quoted_comments = quotePreviousComments(previous_comments);
      var comment_with_context = [];
      comment_with_context.push(snippet);
      if (quoted_comments != '')
        comment_with_context.push(quoted_comments);
      comment_with_context.push('\n' + comment);
      comments_in_context.push(comment_with_context.join('\n'));
    });
    var comment = $('.overallComments textarea').val().trim();
    if (comment != '')
      comment += '\n\n';
    comment += comments_in_context.join('\n\n');
    if (comments_in_context.length > 0)
      comment = 'View in context: ' + window.location + '\n\n' + comment;
    var review_form = $('#reviewform').contents();
    review_form.find('#comment').val(comment);
    review_form.find('#flags select').each(function() {
      var control = findControlForFlag(this);
      if (!control.size())
        return;
      $(this).attr('selectedIndex', control.attr('selectedIndex'));
    });
  }

  $('#preview_comments').live('click', function() {
    fillInReviewForm();
    $('#comment_form').removeClass('inactive');
  });

  $('#post_comments').live('click', function() {
    fillInReviewForm();
    $('#reviewform').contents().find('form').submit();
  });
})();
