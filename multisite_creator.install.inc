<?php
/**
 * @file
 * Installation functions for the Multisite Creator module.
 */

/**
 * Creates the new site directory.
 */
function multisite_creator_create_site_directory($machine_name) {
  $sites_dir = BACKDROP_ROOT . '/sites/' . $machine_name;
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');

  if (!is_dir($sites_dir)) {
    if (!mkdir($sites_dir, 0755, TRUE)) {
      return FALSE;
    }
  }

  // Create necessary subdirectories.
  $subdirs = array('files', 'files/styles', 'modules', 'themes', 'layouts');
  foreach ($subdirs as $subdir) {
    $full_path = $sites_dir . '/' .$subdir;
    if (!is_dir($full_path)) {
      if (!mkdir($full_path, 0755, TRUE)) {
        return FALSE;
      }
    }
  }

  // Create .htaccess for files directory.
  $source_htaccess = BACKDROP_ROOT . '/sites/default/files/.htaccess';
  $target_htaccess = $sites_dir . '/files/.htaccess';

  if (file_exists($source_htaccess)) {
    // Copy the official Backdrop .htaccess.
    if (!copy($source_htaccess, $target_htaccess) && $debug) {
      watchdog(
        'multisite_creator',
        'Could not copy .htaccess to @dir',
        array('@dir' => $target_htaccess),
        WATCHDOG_WARNING
      );
    }
  } else {
    // Fallback with correct Backdrop .htaccess.
    $htaccess_content = "# Turn off all options we don't need.\n";
    $htaccess_content .= "Options -Indexes -ExecCGI -Includes\n";
    $htaccess_content .= "# Set the catch-all handler to prevent scripts from being executed.\n";
    $htaccess_content .= "SetHandler Drupal_Security_Do_Not_Remove_See_SA_2006_006\n";
    $htaccess_content .= "<Files *>\n";
    $htaccess_content .= "  # Override the handler again if we're run later in the evaluation list.\n";
    $htaccess_content .= "  SetHandler Drupal_Security_Do_Not_Remove_See_SA_2013_003\n";
    $htaccess_content .= "</Files>\n";
    $htaccess_content .= "# If we know how to do it safely, disable the PHP engine entirely.\n";
    $htaccess_content .= "<IfModule mod_php5.c>\n";
    $htaccess_content .= "  php_flag engine off\n";
    $htaccess_content .= "</IfModule>\n";
    $htaccess_content .= "<IfModule mod_php7.c>\n";
    $htaccess_content .= "  php_flag engine off\n";
    $htaccess_content .= "</IfModule>\n";
    $htaccess_content .= "<IfModule mod_php.c>\n";
    $htaccess_content .= "  php_flag engine off\n";
    $htaccess_content .= "</IfModule>\n";

    file_put_contents($target_htaccess, $htaccess_content);
  }

  return TRUE;
}

/**
 *
 */
function multisite_creator_create_settings_file($site_data) {
  $sites_dir = BACKDROP_ROOT . '/sites/' . $site_data['machine_name'];
  $settings_file = $sites_dir . '/settings.php';

  // Use the official Backdrop template.
  $template_file = BACKDROP_ROOT . '/sites/default/default.settings.php';

  if (file_exists($template_file)) {
    // Copy the official template.
    return copy($template_file, $settings_file);
  } else {
    // Fallback: minimal structure expected by Backdrop.
    $settings_content = "<?php\n\n";
    $settings_content .= "/**\n * @file\n * Backdrop CMS site configuration file.\n */\n\n";
    $settings_content .= "// \$database = '';\n";
    $settings_content .= "// \$settings = array();\n";

    return file_put_contents($settings_file, $settings_content) !== FALSE;
  }
}

/**
 * Creates the database for the new site.
 */
function multisite_creator_create_database($machine_name) {
  global $settings;
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');

  // Determine database configuration.
  $db_host = $config->get('db_host');

  $db_user = $config->get('db_user');
  $db_password = $settings['msc_db_pass'];
  $db_prefix = $config->get('db_prefix');
  $db_name = $db_prefix . $machine_name;

  try {
    // Connect to MySQL server.
    $connection = new PDO("mysql:host=$db_host", $db_user, $db_password);
    $connection->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

    // Create the database.
    $sql = "CREATE DATABASE IF NOT EXISTS `$db_name` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci";
    $connection->exec($sql);

    return TRUE;
  } catch (PDOException $e) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error creating database @db: @error',
        array('@db' => $db_name, '@error' => $e->getMessage()),
        WATCHDOG_ERROR
      );
    }
    return FALSE;
  }
}

/**
 *
 */
function multisite_creator_prepare_database($machine_name) {
  global $settings;
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');

  // Determine database configuration.
  $db_host = $config->get('db_host');
  $db_user = $config->get('db_user');
  $db_password = $settings['msc_db_pass'];
  $db_prefix = $config->get('db_prefix');

  try {
    $connection = new PDO("mysql:host=$db_host", $db_user, $db_password);
    $connection->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

    // ONLY ensure user has permissions to create this database
    // DO NOT create the database - let the installer do it.
    // Optional: verify user can create databases.
    $test_result = $connection->query("SELECT 1");
    if ($test_result) {
      watchdog(
        'multisite_creator',
        'Database user @user has correct permissions',
        array('@user' => $db_user)
      );
      return TRUE;
    }

    return FALSE;
  } catch (PDOException $e) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error verifying database permissions: @error',
        array('@error' => $e->getMessage()),
        WATCHDOG_ERROR
      );
    }

    return FALSE;
  }
}

/**
 * Adds entry to sites.php file for multisite.
 */
function multisite_creator_update_sites_php($machine_name) {
  $config = config('multisite_creator.settings');
  $base_domain = $config->get('base_domain');
  $domain = $machine_name . '.' . $base_domain;

  $sites_file = BACKDROP_ROOT . '/sites/sites.php';

  // Create sites.php if it doesn't exist.
  if (!file_exists($sites_file)) {
    $sites_content = "<?php\n/**\n * Multisite configuration.\n */\n\n";
  } else {
    $sites_content = file_get_contents($sites_file);
  }

  // Add entry if it doesn't exist.
  $site_entry = "\$sites['$domain'] = '$machine_name';\n";

  // Check if it already exists.
  if (strpos($sites_content, "\$sites['$domain']") === FALSE) {
    $sites_content .= $site_entry;

    if (file_put_contents($sites_file, $sites_content) !== FALSE) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * Removes entry from sites.php.
 */
function multisite_creator_remove_from_sites_php($machine_name) {
  $config = config('multisite_creator.settings');
  $base_domain = $config->get('base_domain');
  $domain = $machine_name . '.' . $base_domain;

  $sites_file = BACKDROP_ROOT . '/sites/sites.php';

  if (!file_exists($sites_file)) {
    return TRUE;
  }

  $sites_content = file_get_contents($sites_file);

  // Remove entries.
  $patterns = array(
    "/\\\$sites\['$domain'\] = '$machine_name';\n?/",
  );

  foreach ($patterns as $pattern) {
    $sites_content = preg_replace($pattern, '', $sites_content);
  }

  return file_put_contents($sites_file, $sites_content) !== FALSE;
}


/**
 *
 */
function multisite_creator_install_backdrop($site_data) {
  global $settings;

  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');

  // Variable configuration.
  $profile = $site_data['profile'];
  $db_host = $config->get('db_host');
  $db_user = $config->get('db_user');
  $db_password = $settings['msc_db_pass']; // From settings.php.
  $db_prefix = $config->get('db_prefix');
  $db_name = $db_prefix . $site_data['machine_name'];
  $base_domain = $config->get('base_domain');
  $site_url = 'http://' . $site_data['machine_name'] . '.' . $base_domain;

  // Build database URL.
  $db_url = sprintf(
    'mysql://%s:%s@%s/%s',
    urlencode($db_user),
    urlencode($db_password),
    $db_host,
    $db_name
  );

  // Build optional extra arguments.
  $extra_args = '';
  if (isset($site_data['extra_fields']) && is_array($site_data['extra_fields'])) {
    foreach ($site_data['extra_fields'] as $form_name => $fields) {
      if (is_array($fields)) {
        foreach ($fields as $field_name => $field_value) {
          if (!empty($field_value)) {
            $extra_args .= ' ' . escapeshellarg($form_name . $field_name . '=' . $field_value);
          }
        }
      }
    }
  }

  // Build install.sh command.
  $install_script = BACKDROP_ROOT . '/core/scripts/install.sh';
  $command = sprintf(
    'cd %s && php %s %s %s --root=%s --url=%s --db-url=%s --site-name=%s --account-name=%s --account-mail=%s --account-pass=%s --site-mail=%s 2>&1',
    escapeshellarg(BACKDROP_ROOT),
    escapeshellarg($install_script),
    escapeshellarg($profile),
    $extra_args,
    escapeshellarg(BACKDROP_ROOT),
    escapeshellarg($site_url),
    escapeshellarg($db_url),
    escapeshellarg($site_data['name']),
    escapeshellarg($site_data['user1_name']),
    escapeshellarg($site_data['user1_email']),
    escapeshellarg($site_data['user1_pass']),
    escapeshellarg($site_data['user1_email'])
  );

  // Execute installation.
  $output = array();
  $return_code = 0;
  exec($command, $output, $return_code);
  $output_text = implode("\n", $output);

  // Log result.
  if ($debug) {
    watchdog(
      'multisite_creator',
      'Install.sh output: @output, return code: @code',
      array('@output' => $output_text, '@code' => $return_code . $command)
    );
  }

  if ($return_code === 0 && strpos($output_text, 'Backdrop installed successfully') !== FALSE) {
    return TRUE;
  } else {
    return FALSE;
  }
}


/**
 * Creates Apache configuration for the new site.
 */
function multisite_creator_create_apache_config($machine_name) {
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');
  $apache_config_prefix = $config->get('apache_config_prefix');
  $template = $config->get('apache_template');
  $base_domain = $config->get('base_domain');
  $domain = $machine_name . '.' . $base_domain;

  // Create log directories if enabled.
  if ($config->get('create_log_directories')) {
    if (!multisite_creator_create_log_directories($machine_name)) {
      if ($debug) {
        watchdog(
          'multisite_creator',
          'Failed to create log directories for @site',
          array('@site' => $machine_name),
          WATCHDOG_ERROR
        );
      }
      return FALSE;
    }
  }

  // Replace variables in template.
  $apache_config = str_replace('{MACHINE_NAME}', $machine_name, $template);
  $apache_config = str_replace('{DOMAIN}', $domain, $apache_config);

  // Write configuration using temporary file.
  $path = $config->get('apache_config_path');
  $config_file = $path . '/' . $apache_config_prefix . $machine_name . '.conf';
  $temp_file = '/tmp/apache_' . $machine_name . '.conf';

  try {
    // Write to temporary file first.
    if (file_put_contents($temp_file, $apache_config) === FALSE) {
      throw new Exception('Could not create temporary file');
    }

    // Copy with sudo to final directory.
    $copy_command = sprintf(
      'sudo cp %s %s',
      escapeshellarg($temp_file),
      escapeshellarg($config_file)
    );

    $output = array();
    $return_code = 0;
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Executing command: @cmd',
        array('@cmd' => $copy_command),
        WATCHDOG_INFO
      );
    }
    exec($copy_command . ' 2>&1', $output, $return_code);
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Command output: @output, return code: @code',
        array('@output' => implode(' ', $output), '@code' => $return_code),
        WATCHDOG_INFO
      );
    }
    // Clean up temporary file.
    unlink($temp_file);

    if ($return_code !== 0) {
      throw new Exception('Could not copy configuration: ' . implode(' ', $output));
    }
  } catch (Exception $e) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error creating Apache configuration file @file: @error',
        array('@file' => $machine_name . '.conf', '@error' => $e->getMessage()),
        WATCHDOG_ERROR
      );
    }
    return FALSE;
  }

  // Enable the site
  // TODO: if log directories are created in config, they must be created before
  // restarting apache, otherwise it gives error.
  $command = $config->get('apache_ensite_command');
  if (!empty($command)) {
    $command = $command . ' ' . escapeshellarg($apache_config_prefix . $machine_name . '.conf');
    $output = array();
    $return_code = 0;

    try {
      exec($command . ' 2>&1', $output, $return_code);

      if ($return_code !== 0) {
        throw new Exception('Enable site command failed: ' . implode(' ', $output));
      }
    } catch (Exception $e) {
      if ($debug) {
        watchdog(
          'multisite_creator',
          'Error enabling site @site: @error',
          array('@site' => $machine_name, '@error' => $e->getMessage()),
          WATCHDOG_ERROR
        );
      }
      return FALSE;
    }

    return TRUE;
  } else {
    if (!multisite_creator_reload_apache()) {
      throw new Exception('Could not reload Apache');
    }
    return TRUE;
  }
}

/**
 * Restarts Apache.
 */
function multisite_creator_reload_apache() {

  $config = config('multisite_creator.settings');
  $command = $config->get('apache_reload_command');
  $output = array();
  $return_code = 0;
  exec($command . ' 2>&1', $output, $return_code);

  return $return_code === 0;
}

/**
 * Drops a database.
 */
function multisite_creator_drop_database($machine_name) {
  global $settings;
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');

  $db_host = $config->get('db_host');
  $db_user = $config->get('db_user');
  $db_password = $settings['msc_db_pass'];
  $db_prefix = $config->get('db_prefix');
  $db_name = $db_prefix . $machine_name;

  try {
    $connection = new PDO("mysql:host=$db_host", $db_user, $db_password);
    $connection->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

    $sql = "DROP DATABASE IF EXISTS `$db_name`";
    $connection->exec($sql);

    return TRUE;
  } catch (PDOException $e) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error dropping database @db: @error',
        array('@db' => $db_name, '@error' => $e->getMessage()),
        WATCHDOG_ERROR
      );
    }
    return FALSE;
  }
}

/**
 * Creates log directories for a new site.
 */
function multisite_creator_create_log_directories($machine_name) {
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');
  $log_path = $config->get('apache_log_path');
  $log_dir_template = $config->get('log_directory_template') ?: '{MACHINE_NAME}';

  if (empty($log_path)) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Cannot create log directories: apache_log_path not configured',
        array(),
        WATCHDOG_WARNING
      );
    }
    return FALSE;
  }

  // Replace variables in directory name.
  $log_dir_name = str_replace('{MACHINE_NAME}', $machine_name, $log_dir_template);
  $full_log_path = rtrim($log_path, '/') . '/' . $log_dir_name;

  // Create directory.
  $mkdir_command = sprintf(
    'mkdir %s',
    escapeshellarg($full_log_path)
  );

  $output = array();
  $return_code = 0;

  if ($debug) {
    watchdog(
      'multisite_creator',
      'Creating log directory: @path',
      array('@path' => $full_log_path),
      WATCHDOG_INFO
    );
  }

  exec($mkdir_command . ' 2>&1', $output, $return_code);

  if ($return_code !== 0) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error creating log directory @path: @error',
        array('@path' => $full_log_path, '@error' => implode(' ', $output)),
        WATCHDOG_ERROR
      );
    }
    return FALSE;
  }

  if ($debug) {
    watchdog(
      'multisite_creator',
      'Successfully created log directory @path',
      array('@path' => $full_log_path),
      WATCHDOG_INFO
    );
  }

  return TRUE;
}

/**
 * Deletes log directories of a site.
 */
function multisite_creator_delete_log_directories($machine_name) {
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');
  $log_path = $config->get('apache_log_path');
  $log_dir_template = $config->get('log_directory_template') ?: '{MACHINE_NAME}';

  // Replace variables in directory name.
  $log_dir_name = str_replace('{MACHINE_NAME}', $machine_name, $log_dir_template);
  $full_log_path = rtrim($log_path, '/') . '/' . $log_dir_name;

  $rm_command = sprintf(
    'sudo rm -rf %s',
    escapeshellarg($full_log_path)
  );

  $output = array();
  $return_code = 0;

  exec($rm_command . ' 2>&1', $output, $return_code);

  if ($return_code !== 0) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error deleting log directory @path: @error',
        array('@path' => $full_log_path, '@error' => implode(' ', $output)),
        WATCHDOG_ERROR
      );
    }
    return FALSE;
  }
}
