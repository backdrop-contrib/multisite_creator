<?php
/**
 * @file
 * Maintenance and cron tasks for the Multisite Creator module.
 */

/**
 * Implements hook_cron().
 */
function multisite_creator_cron() {
  $config = config('multisite_creator.settings');

  // Send weekly summary on Sundays.
  if (date('w') == 0) { // Sunday.
    $last_weekly_summary = $config->get('multisite_creator_last_weekly_summary');
    if (time() - $last_weekly_summary > 518400) { // 6 days
      if (multisite_creator_send_weekly_summary()) {
        $config->set('multisite_creator_last_weekly_summary', time());
      }
    }
  }

  multisite_creator_process_queue();
}

/**
 * Calculates the size of a directory.
 */
function multisite_creator_get_directory_size($directory) {
  $size = 0;

  if (!is_dir($directory)) {
    return 0;
  }

  $iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($directory));

  foreach ($iterator as $file) {
    if ($file->isFile()) {
      $size += $file->getSize();
    }
  }

  return $size;
}
