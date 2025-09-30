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

  // Check SSL certificates daily
  // let's disable this, as certbot takes care of cert renewing
  // $last_ssl_check = $config->get('multisite_creator_last_ssl_check');
  // if (time() - $last_ssl_check > 86400) { // 24 hours
  //   multisite_creator_cron_check_ssl();
  //   $config->set('multisite_creator_last_ssl_check', time());
  // }

  // Send weekly summary on Sundays
  if (date('w') == 0) { // Sunday
    $last_weekly_summary = $config->get('multisite_creator_last_weekly_summary');
    if (time() - $last_weekly_summary > 518400) { // 6 days
      if (multisite_creator_send_weekly_summary()) {
        $config->set('multisite_creator_last_weekly_summary', time());
      }
    }
  }

  // Clean old logs (weekly)
  // $last_log_cleanup = $config->get('multisite_creator_last_log_cleanup');
  // if (time() - $last_log_cleanup > 604800) { // 7 days
  //   multisite_creator_cron_cleanup_logs();
  //   $config->set('multisite_creator_last_log_cleanup', time());
  // }

  multisite_creator_process_queue();
}

/**
 * Checks SSL certificates status.
 */
// function multisite_creator_cron_check_ssl() {
//   $config = config('multisite_creator.settings');

//   if (!$config->get('ssl_enabled')) {
//     return;
//   }

//   $issues = multisite_creator_get_ssl_issues();

//   // Send notification if there are problems
//   if (!empty($issues) && $config->get('email_notifications')) {
//     multisite_creator_send_ssl_status_notification();
//   }

//   // Try to renew certificates expiring soon
//   foreach ($issues as $machine_name => $issue) {
//     if ($issue['status'] === 'expiring_soon' && isset($issue['days_until_expire']) && $issue['days_until_expire'] <= 7) {
//       multisite_creator_cron_renew_certificate($machine_name);
//     }
//   }
// }

/**
 * Attempts to renew a specific certificate.
 */
// function multisite_creator_cron_renew_certificate($machine_name) {
//   $config = config('multisite_creator.settings');
//   $certbot_path = $config->get('certbot_path');
//   $base_domain = $config->get('base_domain');
//   $domain = $machine_name . '.' . $base_domain;

//   if (!file_exists($certbot_path)) {
//     return FALSE;
//   }

//   // Execute renewal
//   $command = sprintf(
//     '%s renew --cert-name %s --quiet',
//     escapeshellarg($certbot_path),
//     escapeshellarg($domain)
//   );

//   $output = array();
//   $return_code = 0;
//   exec($command . ' 2>&1', $output, $return_code);

//   if ($return_code === 0) {
//     // Restart Apache to apply new certificate
//     multisite_creator_reload_apache();

//     watchdog(
//       'multisite_creator',
//       'SSL certificate automatically renewed for @domain',
//       array('@domain' => $domain),
//       WATCHDOG_INFO
//     );

//     return TRUE;
//   } else {
//     watchdog(
//       'multisite_creator',
//       'Error renewing SSL certificate for @domain: @output',
//       array('@domain' => $domain, '@output' => implode("\n", $output)),
//       WATCHDOG_WARNING
//     );

//     return FALSE;
//   }
// }

/**
 * Cleans old module logs.
 */
// function multisite_creator_cron_cleanup_logs() {
//   // Delete logs older than 3 months
//   $three_months_ago = time() - (90 * 24 * 60 * 60);

//   $deleted = db_delete('watchdog')
//     ->condition('type', 'multisite_creator')
//     ->condition('timestamp', $three_months_ago, '<')
//     ->execute();

//   if ($deleted > 0) {
//     watchdog(
//       'multisite_creator',
//       'Cleaned @count old logs',
//       array('@count' => $deleted),
//       WATCHDOG_INFO
//     );
//   }
// }

/**
 * Checks disk space of sites.
 */
// function multisite_creator_cron_check_disk_space() {
//   $sites_dir = BACKDROP_ROOT . '/sites';
//   $warning_threshold = 1024 * 1024 * 1024; // 1GB
//   $sites_with_issues = array();

//   if (!is_dir($sites_dir)) {
//     return;
//   }

//   $dirs = scandir($sites_dir);
//   foreach ($dirs as $dir) {
//     if ($dir === '.' || $dir === '..' || $dir === 'all' || $dir === 'default') {
//       continue;
//     }

//     $full_path = $sites_dir . '/' . $dir;
//     if (!is_dir($full_path)) {
//       continue;
//     }

//     // Check if it has settings.php (is a valid site)
//     if (!file_exists($full_path . '/settings.php')) {
//       continue;
//     }

//     // Calculate directory size
//     $size = multisite_creator_get_directory_size($full_path);

//     if ($size > $warning_threshold) {
//       $sites_with_issues[$dir] = $size;
//     }
//   }

//   // Send notification if there are sites taking up too much space
//   if (!empty($sites_with_issues)) {
//     multisite_creator_send_disk_space_notification($sites_with_issues);
//   }
// }

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

/**
 * Sends disk space notification.
 */
// function multisite_creator_send_disk_space_notification($sites_with_issues) {
//   module_load_include('inc', 'multisite_creator', 'multisite_creator.mail');

//   $config = config('multisite_creator.settings');

//   if (!$config->get('email_notifications')) {
//     return FALSE;
//   }

//   $notification_emails = $config->get('notification_emails');
//   if (empty($notification_emails)) {
//     return FALSE;
//   }

//   $emails = multisite_creator_parse_email_list($notification_emails);
//   $from_email = $config->get('email_from');

//   // Prepare content
//   $subject = t('Warning: Sites taking up large disk space');

//   $message = array();
//   $message[] = t('The following sites are using more than 1GB of space:');
//   $message[] = '';

//   foreach ($sites_with_issues as $machine_name => $size) {
//     $size_mb = round($size / (1024 * 1024), 2);
//     $message[] = t('• @site: @size MB', array(
//       '@site' => $machine_name,
//       '@size' => $size_mb
//     ));
//   }

//   $message[] = '';
//   $message[] = t('Consider reviewing these sites to optimize space usage.');
//   $message[] = t('Report date: @date', array('@date' => date('Y-m-d H:i:s')));
//   $message[] = '';
//   $message[] = t('--');
//   $message[] = t('This email was automatically generated by the Multisite Creator module.');

//   $body = implode("\n", $message);

//   // Send email
//   $sent_count = 0;
//   foreach ($emails as $email) {
//     if (multisite_creator_send_email($email, $from_email, $subject, $body)) {
//       $sent_count++;
//     }
//   }

//   return $sent_count > 0;
// }

/**
 * Checks overall system health.
 */
// function multisite_creator_cron_system_health_check() {
//   global $settings;
//   $issues = array();

//   // Check sites directory permissions
//   $sites_dir = BACKDROP_ROOT . '/sites';
//   if (!is_writable($sites_dir)) {
//     $issues[] = t('Directory @dir is not writable', array('@dir' => $sites_dir));
//   }

//   // Check database connection
//   $config = config('multisite_creator.settings');

//   $db_host = $config->get('db_host');
//   $db_user = $config->get('db_user');
//   $db_password = $settings['msc_db_pass'];

//   try {
//     $connection = new PDO("mysql:host=$db_host", $db_user, $db_password);
//     $connection = null; // Close connection
//   } catch (PDOException $e) {
//     $issues[] = t('Cannot connect to database: @error', array('@error' => $e->getMessage()));
//   }

//   // Check Apache status
//   $apache_status = multisite_creator_check_apache_status();
//   if (!$apache_status) {
//     $issues[] = t('Apache is not responding correctly');
//   }

//   // Check system disk space
//   $disk_usage = disk_free_space('/');
//   if ($disk_usage < 1024 * 1024 * 1024) { // Less than 1GB free
//     $issues[] = t(
//       'Low disk space: @space MB free',
//       array('@space' => round($disk_usage / (1024 * 1024), 2))
//     );
//   }

//   // Send notification if there are problems
//   if (!empty($issues)) {
//     multisite_creator_send_system_health_notification($issues);
//   }

//   return empty($issues);
// }

/**
 * Checks Apache status.
 */
// function multisite_creator_check_apache_status() {
//   $output = array();
//   $return_code = 0;
//   exec('systemctl is-active apache2 2>/dev/null', $output, $return_code);

//   return $return_code === 0 && !empty($output) && trim($output[0]) === 'active';
// }

/**
 * Sends system health notification.
 */
// function multisite_creator_send_system_health_notification($issues) {
//   module_load_include('inc', 'multisite_creator', 'multisite_creator.mail');

//   $config = config('multisite_creator.settings');

//   if (!$config->get('email_notifications')) {
//     return FALSE;
//   }

//   $notification_emails = $config->get('notification_emails');
//   if (empty($notification_emails)) {
//     return FALSE;
//   }

//   $emails = multisite_creator_parse_email_list($notification_emails);
//   $from_email = $config->get('email_from');

//   // Prepare content
//   $subject = t('URGENT: Multisite Creator system issues');

//   $message = array();
//   $message[] = t('The following system issues have been detected:');
//   $message[] = '';

//   foreach ($issues as $issue) {
//     $message[] = t('• @issue', array('@issue' => $issue));
//   }

//   $message[] = '';
//   $message[] = t('Please review the system as soon as possible.');
//   $message[] = t('Report date: @date', array('@date' => date('Y-m-d H:i:s')));
//   $message[] = t('Server: @host', array('@host' => $_SERVER['HTTP_HOST']));
//   $message[] = '';
//   $message[] = t('--');
//   $message[] = t('This email was automatically generated by the Multisite Creator module.');

//   $body = implode("\n", $message);

//   // Send email with high priority
//   $sent_count = 0;
//   foreach ($emails as $email) {
//     if (multisite_creator_send_email($email, $from_email, $subject, $body)) {
//       $sent_count++;
//     }
//   }

//   if ($sent_count > 0) {
//     watchdog(
//       'multisite_creator',
//       'Urgent notification sent for system issues',
//       array(),
//       WATCHDOG_CRITICAL
//     );
//   }

//   return $sent_count > 0;
// }
