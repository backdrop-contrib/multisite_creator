<?php

/**
 * @file
 * Functions to manage SSL certificates with Let's Encrypt.
 */

/**
 * Creates an SSL certificate with Let's Encrypt for the new site. We had some
 * problems with this function in batch processing. Let's Encrypt servers
 * returned, only for some of the sites, a 405 "Not Allowed" error (due to rate
 * limits??). This function is unused to try a different approach: use wildcard
 * certifcate creation for all the subdomains of the main backdrop installation.
 *
 * See multisite_creator_add_ssl_certificate().
 *
 * @param string $machine_name
 *   Site machine name.
 *
 * @return bool|string
 *   TRUE if created successfully, string with error if there are problems.
 */
function multisite_creator_create_ssl_certificate($machine_name) {
  $config = config('multisite_creator.settings');

  $certbot_path = $config->get('certbot_path');
  $ssl_email = $config->get('ssl_email');
  $base_domain = $config->get('base_domain');
  $domain = $machine_name . '.' . $base_domain;

  // Verify that certbot exists
  if (!file_exists($certbot_path) || !is_executable($certbot_path)) {
    return 'Certbot is not installed or is not executable';
  }

  // Verify that the domain resolves correctly
  if (!multisite_creator_check_domain_dns($domain)) {
    return 'Domain ' . $domain . ' does not resolve correctly. Configure DNS first.';
  }

  try {
    // Create certificate with certbot
    $result = multisite_creator_run_certbot($domain, $ssl_email, $certbot_path);
    if ($result !== TRUE) {
      return $result;
    }

    // Restart Apache to apply SSL changes
    if (!multisite_creator_reload_apache()) {
      return 'Could not restart Apache after configuring SSL';
    }

    watchdog('multisite_creator', 'SSL certificate created for @domain', array('@domain' => $domain));

    return TRUE;
  } catch (Exception $e) {
    return 'Error creating SSL certificate: ' . $e->getMessage();
  }
}

/**
 * Adds an SSL certificate from a template for the new site. It uses the avility
 * of Let's Encrypt to create an SSL certificate for all subdomains of a domain
 * with wildcards.
 *
 * See multisite_creator_create_apache_config().
 *
 * @param string $machine_name
 *   Site machine name.
 *
 * @return bool|string
 *   TRUE if created successfully, string with error if there are problems.
 */
function multisite_creator_add_ssl_certificate($machine_name) {
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');
  $apache_config_prefix = $config->get('apache_config_prefix');
  $template = $config->get('apache_ssl_template');
  $base_domain = $config->get('base_domain');
  $domain = $machine_name . '.' . $base_domain;

  // Replace variables in template
  $apache_config = str_replace('{MACHINE_NAME}', $machine_name, $template);
  $apache_config = str_replace('{DOMAIN}', $domain, $apache_config);

  // Write configuration using temporary file
  $path = $config->get('apache_config_path');
  $config_file = $path . '/' . $apache_config_prefix . $machine_name . '-le-ssl.conf';
  $temp_file = '/tmp/apache_ssl_' . $machine_name . '.conf';

  try {
    // Write to temporary file first
    if (file_put_contents($temp_file, $apache_config) === FALSE) {
      throw new Exception('Could not create temporary file');
    }

    // Copy with sudo to final directory
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
    // Clean up temporary file
    unlink($temp_file);

    if ($return_code !== 0) {
      throw new Exception('Could not copy configuration: ' . implode(' ', $output));
    }
  } catch (Exception $e) {
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error creating Apache SSL configuration file @file: @error',
        array('@file' => $machine_name . '.conf', '@error' => $e->getMessage()),
        WATCHDOG_ERROR
      );
    }
    return FALSE;
  }

  // Enable the site
  // TODO: if log directories are created in config, they must be created before
  // restarting apache, otherwise it gives error
  $command = $config->get('apache_ensite_command');
  if (!empty($command)) {
    $command = $command . ' ' . escapeshellarg($apache_config_prefix . $machine_name . '-le-ssl.conf');
    $output = array();
    $return_code = 0;

    try {
      exec($command . ' 2>&1', $output, $return_code);

      if ($return_code !== 0) {
        throw new Exception('Enable SSL site command failed: ' . implode(' ', $output));
      }
    } catch (Exception $e) {
      if ($debug) {
        watchdog(
          'multisite_creator',
          'Error enabling SSL site @site: @error',
          array('@site' => $machine_name, '@error' => $e->getMessage()),
          WATCHDOG_ERROR
        );
      }
      return FALSE;
    }

    return TRUE;
  } else {
    return TRUE;
  }
}

/**
 * Runs certbot to create the certificate.
 */
function multisite_creator_run_certbot($domain, $email, $certbot_path) {
  $config = config('multisite_creator.settings');
  $debug = $config->get('debug');
  // Check if certificate already exists
  if (multisite_creator_ssl_certificate_exists($domain)) {
    return TRUE; // Already exists, nothing to do
  }

  // Build certbot command
  $command = sprintf(
    'sudo %s --apache -m %s --agree-tos -n -d %s',
    escapeshellarg($certbot_path),
    escapeshellarg($email),
    escapeshellarg($domain)
  );

  $output = array();
  $return_code = 0;
  exec($command . ' 2>&1', $output, $return_code);

  if ($return_code === 0) {
    return TRUE;
  } else {
    $error_output = implode("\n", $output);
    if ($debug) {
      watchdog(
        'multisite_creator',
        'Error running certbot: @output',
        array('@output' => $error_output),
        WATCHDOG_ERROR
      );
    }
    return 'Certbot error: ' . $error_output;
  }
}

/**
 * Checks if an SSL certificate already exists for the domain.
 */
function multisite_creator_ssl_certificate_exists($domain) {
  $cert_path = '/etc/letsencrypt/live/' . $domain . '/fullchain.pem';
  return file_exists($cert_path);
}

/**
 * Checks that the domain resolves correctly.
 */
function multisite_creator_check_domain_dns($domain) {
  // Get current server IP
  $server_ip = multisite_creator_get_server_ip();

  // Resolve the domain
  $domain_ip = gethostbyname($domain);

  // If gethostbyname returns the same string, it couldn't resolve
  if ($domain_ip === $domain) {
    return FALSE;
  }

  // Check if domain points to current server
  return $domain_ip === $server_ip;
}

/**
 * Gets the server's public IP.
 */
function multisite_creator_get_server_ip() {
  // Try different methods to get IP
  $methods = array(
    'curl -s https://api.ipify.org',
    'curl -s https://icanhazip.com',
    'wget -qO- https://api.ipify.org',
  );

  foreach ($methods as $method) {
    $output = array();
    $return_code = 0;
    exec($method . ' 2>/dev/null', $output, $return_code);

    if ($return_code === 0 && !empty($output[0])) {
      $ip = trim($output[0]);
      if (filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4)) {
        return $ip;
      }
    }
  }

  // Fallback: use network interface IP
  $output = array();
  exec("hostname -I | awk '{print $1}'", $output);
  if (!empty($output[0])) {
    return trim($output[0]);
  }

  return '127.0.0.1'; // Last option
}


/**
 * Removes SSL certificates when deleting a site.
 */
function multisite_creator_cleanup_ssl_certificate($machine_name) {
  $config = config('multisite_creator.settings');
  $apache_config_prefix = $config->get('apache_config_prefix');
  $base_domain = $config->get('base_domain');
  $domain = $machine_name . '.' . $base_domain;
  $certbot_path = $config->get('certbot_path');

  if (!file_exists($certbot_path)) {
    return FALSE;
  }

  // Remove certificate with certbot
  $command = sprintf(
    'sudo %s delete --cert-name %s -n',
    escapeshellarg($certbot_path),
    escapeshellarg($domain)
  );

  $output = array();
  $return_code = 0;
  exec($command . ' 2>&1', $output, $return_code);

  // Remove Apache SSL configuration
  $ssl_config_file = '/etc/apache2/sites-available/' . $apache_config_prefix . $machine_name . '-ssl.conf';
  if (file_exists($ssl_config_file)) {
    // Disable SSL site
    exec('a2dissite ' . escapeshellarg($machine_name . '-ssl.conf') . ' 2>/dev/null');
    unlink($ssl_config_file);
  }

  return $return_code === 0;
}

/**
 * Checks SSL certificate status.
 */
function multisite_creator_check_ssl_status($machine_name) {
  $config = config('multisite_creator.settings');
  $base_domain = $config->get('base_domain');
  $domain = $machine_name . '.' . $base_domain;

  $cert_path = '/etc/letsencrypt/live/' . $domain . '/fullchain.pem';

  if (!file_exists($cert_path)) {
    return array(
      'status' => 'no_certificate',
      'message' => t('No SSL certificate'),
    );
  }

  // Check expiration date
  $cert_info = openssl_x509_parse(file_get_contents($cert_path));
  if ($cert_info === FALSE) {
    return array(
      'status' => 'error',
      'message' => t('Error reading certificate'),
    );
  }

  $expire_date = $cert_info['validTo_time_t'];
  $days_until_expire = floor(($expire_date - time()) / 86400);

  if ($days_until_expire < 0) {
    return array(
      'status' => 'expired',
      'message' => t('Certificate expired'),
      'expire_date' => date('Y-m-d', $expire_date),
    );
  } elseif ($days_until_expire < 30) {
    return array(
      'status' => 'expiring_soon',
      'message' => t("Expires in @days days", array('@days' => $days_until_expire)),
      'expire_date' => date('Y-m-d', $expire_date),
      'days_until_expire' => $days_until_expire,
    );
  } else {
    return array(
      'status' => 'valid',
      'message' => t("Valid until @date", array('@date' => date('Y-m-d', $expire_date))),
      'expire_date' => date('Y-m-d', $expire_date),
      'days_until_expire' => $days_until_expire,
    );
  }
}
