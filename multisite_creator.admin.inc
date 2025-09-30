<?php

/**
 * @file
 * Administration functions for the Multisite Creator module.
 */

/**
 * Main form to create a new site.
 */
function multisite_creator_form($form, &$form_state) {
  global $settings;
  require_once './core/includes/install.core.inc';


  $form['site_info'] = array(
    '#type' => 'fieldset',
    '#title' => t('New site information'),
    '#collapsible' => FALSE,
  );

  $form['site_info']['site_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Site name'),
    '#description' => t('Name that will appear as the website title.'),
    '#required' => TRUE,
    '#maxlength' => 255,
  );

  $profiles = install_find_profiles();
  $profile_options = array();
  foreach ($profiles as $profile => $value) {
    $profile_options[$profile] = $value->name;
  }

  $form['site_info']['profile'] = array(
    '#type' => 'select',
    '#title' => t('Profile'),
    '#options' => $profile_options,
    '#description' => t('Profile to install with.'),
    '#required' => TRUE,
    '#ajax' => array(
      'callback' => 'multisite_creator_profile_fields_callback',
      'wrapper' => 'profile-extra-fields',
      'method' => 'replace',
    ),
  );

  $form['site_info']['machine_name'] = array(
    '#type' => 'machine_name',
    '#title' => t('Subdomain'),
    '#maxlength' => 50,
    '#description' => t('Site subdomain. Can only contain lowercase letters, numbers and hyphens.'),
    '#default_value' => NULL,
    '#machine_name' => array(
      'exists' => 'multisite_creator_site_exists',
      'source' => array('site_info', 'site_name'),
      'label' => t('Subdomain'),
      'replace_pattern' => '[^a-z0-9-]+',
      'replace' => '-',
    ),
    '#required' => TRUE,
  );


  $form['user1_info'] = array(
    '#type' => 'fieldset',
    '#title' => t('Site administrator'),
    '#collapsible' => FALSE,
  );

  if (!isset($settings['msc_user1_email'])) {
    $form['user1_info']['user1_email'] = array(
      '#type' => 'email',
      '#title' => t('Email address'),
      '#description' => t('Email address for the new site administrator.'),
      '#required' => TRUE,
    );
  } else {
    $form['user1_info']['user1_email'] = array(
      '#type' => 'item',
      '#title' => t('Email address'),
      '#description' => t('The site administrator email is preconfigured in settings.php'),
    );
  }

  if (!isset($settings['msc_user1_name'])) {
    $form['user1_info']['user1_name'] = array(
      '#type' => 'textfield',
      '#title' => t('Username'),
      '#description' => t('Username for the site administrator.'),
      '#required' => TRUE,
      '#maxlength' => 60,
    );
  } else {
    $form['user1_info']['user1_name'] = array(
      '#type' => 'item',
      '#title' => t('Username'),
      '#description' => t('The site administrator username is preconfigured in settings.php'),
    );
  }

  if (!isset($settings['msc_user1_pass'])) {
    $form['user1_info']['user1_pass'] = array(
      '#type' => 'password',
      '#title' => t('Password'),
      '#description' => t('Password for the site administrator.'),
      '#required' => TRUE,
      '#password_strength' => TRUE,
      '#password_toggle' => TRUE,
      '#element_validate' => array('user_password_policy_validate'),
    );

    $form['user1_info']['user1_pass_confirm'] = array(
      '#type' => 'password',
      '#title' => t('Confirm password'),
      '#description' => t('Repeat the password to confirm it.'),
      '#required' => TRUE,
    );
  } else {
    $form['user1_info']['user1_pass'] = array(
      '#type' => 'item',
      '#title' => t('Password'),
      '#description' => t('The administrator password is preconfigured in settings.php'),
    );
  }

  // Container for profile-specific fields
  $form['profile_extra_fields'] = array(
    '#type' => 'container',
    '#prefix' => '<div id="profile-extra-fields">',
    '#suffix' => '</div>',
  );

  // Add fields from selected profile
  $selected_profile = isset($form_state['values']['profile']) ? $form_state['values']['profile'] : '';
  if (!empty($selected_profile)) {
    multisite_creator_add_profile_fields($form, $form_state, $selected_profile);
  }

  $form['actions'] = array(
    '#type' => 'actions',
  );

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create site'),
    '#weight' => 10,
  );

  $form['#attached']['css'][] = array(
    'data' => '#edit-site-name-machine-name-suffix .admin-link { display: none; }',
    'type' => 'inline',
  );

  return $form;
}

/**
 * Validation for the creation form.
 */
function multisite_creator_form_validate($form, &$form_state) {
  global $settings;
  $values = $form_state['values'];
  $machine_name = $values['machine_name'];

  // Check if already exists
  if (multisite_creator_site_exists($machine_name)) {
    form_set_error('machine_name', t('A site with this subdomain already exists.'));
  }

  // Validate username
  if (isset($values['user1_name']) && strlen($values['user1_name']) < 3) {
    form_set_error('user1_name', t('Username must be at least 3 characters long.'));
  }

  // Validate passwords match
  if (!isset($settings['msc_user1_pass'])) {
    if ($values['user1_pass'] !== $values['user1_pass_confirm']) {
      form_set_error('user1_pass_confirm', t('Passwords do not match.'));
    }
  }

  // Load profile and call its validation
  if (!empty($values['profile'])) {
    require_once BACKDROP_ROOT . '/core/includes/install.inc';
    $profile = $values['profile'];
    $profile_file = _install_find_profile_file($profile, 'profile');
    if ($profile_file) {
      include_once $profile_file;
    }

    $validation_function = $profile . '_multisite_creator_form_validate';
    if (function_exists($validation_function)) {
      $validation_function($form, $form_state);
    }
  }
}

/**
 * Processing of the creation form.
 */
function multisite_creator_form_submit($form, &$form_state) {
  global $settings;
  $values = $form_state['values'];

  // Build extra_fields parsing field names with '__' prefix
  $extra_fields = array();

  foreach ($values as $field_name => $field_value) {
    // Look for fields containing '__' (form__field separator)
    if (strpos($field_name, '__') !== FALSE && !empty($field_value)) {
      list($form_name, $real_field_name) = explode('__', $field_name, 2);
      $extra_fields[$form_name][$real_field_name] = $field_value;
    }
  }

  $site_data = array(
    'name' => $values['site_name'],
    'machine_name' => $values['machine_name'],
    'profile' => $values['profile'],
    'user1_email' => isset($settings['msc_user1_email']) ? $settings['msc_user1_email'] : $values['user1_email'],
    'user1_name' => isset($settings['msc_user1_name']) ? $settings['msc_user1_name'] : $values['user1_name'],
    'user1_pass' => isset($settings['msc_user1_pass']) ? $settings['msc_user1_pass'] : $values['user1_pass'],
    'extra_fields' => $extra_fields,
  );

  // Always add to queue - no direct processing
  $queue_id = multisite_creator_add_to_queue($site_data);
  $queue_status = multisite_creator_get_queue_status();
  $pending_count = isset($queue_status['pending']) ? $queue_status['pending'] : 0;

  backdrop_set_message(t(
    'Site creation queued successfully. Queue position: @pos, ID: @id. The site will be created automatically.',
    array('@pos' => $pending_count, '@id' => $queue_id)
  ));

  $form_state['redirect'] = 'admin/config/system/multisite-creator';

  // multisite_creator_check_stale_lock();

  // if (multisite_creator_is_creation_in_progress()) {
  //   // Add to queue instead of showing error
  //   $queue_id = multisite_creator_add_to_queue($site_data);
  //   $queue_status = multisite_creator_get_queue_status();
  //   $pending_count = isset($queue_status['pending']) ? $queue_status['pending'] : 0;

  //   backdrop_set_message(t(
  //     'Hi ha una creació en curs. El teu lloc s\'ha afegit a la cua (posició @pos). ID: @id',
  //     array('@pos' => $pending_count, '@id' => $queue_id)
  //   ));

  //   $form_state['redirect'] = 'admin/config/system/multisite-creator/manage';
  //   return;
  // }

  // multisite_creator_set_creation_lock();
  // $result = multisite_creator_create_site($site_data);
  // multisite_creator_release_creation_lock();

  // if ($result === TRUE) {
  //   $site_url = multisite_creator_get_site_url($site_data['machine_name']);
  //   backdrop_set_message(t(
  //     'Site "@name" has been created successfully. You can access it at: <a href="@url" target="_blank">@url</a>',
  //     array('@name' => $site_data['name'], '@url' => $site_url)
  //   ));

  //   // Redirect to configuration page
  //   $form_state['redirect'] = 'admin/config/system/multisite-creator';
  // } else {
  //   backdrop_set_message(t('Error creating site: @error', array('@error' => $result)), 'error');
  // }
}

/**
 * AJAX callback to update profile fields.
 */
function multisite_creator_profile_fields_callback($form, $form_state) {
  return $form['profile_extra_fields'];
}

/**
 * Adds specific fields from the selected profile.
 */
function multisite_creator_add_profile_fields(&$form, &$form_state, $profile) {
  // Manually load the .profile file
  $profile_file = BACKDROP_ROOT . '/profiles/' . $profile . '/' . $profile . '.profile';

  if (file_exists($profile_file)) {
    include_once $profile_file;
  }

  // Call profile-specific function to add fields
  $function = $profile . '_multisite_creator_form_alter';

  if (function_exists($function)) {
    $function($form['profile_extra_fields'], $form_state);
  }
}

/**
 * Module configuration form.
 */
function multisite_creator_settings_form($form, &$form_state) {
  $config = config('multisite_creator.settings');
  $form['#config'] = 'multisite_creator.settings';

  $form['settings_tabs'] = array(
    '#type' => 'vertical_tabs',
  );

  $form['database'] = array(
    '#type' => 'fieldset',
    '#title' => t('Database configuration'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#group' => 'settings_tabs',
  );

  $form['database']['db_host'] = array(
    '#type' => 'textfield',
    '#title' => t('Database server'),
    '#default_value' => $config->get('db_host'),
    '#description' => t('Database server IP address.'),
  );

  $form['database']['db_user'] = array(
    '#type' => 'textfield',
    '#title' => t('Database user'),
    '#default_value' => $config->get('db_user'),
    '#required' => TRUE,
  );

  $form['database']['db_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Database prefix'),
    '#default_value' => $config->get('db_prefix'),
    '#description' => t('Prefix for new site database names.'),
  );

  $form['apache'] = array(
    '#type' => 'fieldset',
    '#title' => t('Apache configuration'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#group' => 'settings_tabs',
  );

  $form['apache']['base_domain'] = array(
    '#type' => 'textfield',
    '#title' => t('Base domain'),
    '#default_value' => $config->get('base_domain'),
    '#description' => t('Base domain for new sites (example: site1.example.com).'),
    '#required' => TRUE,
  );

  $form['apache']['apache_config_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Apache config file prefix'),
    '#default_value' => $config->get('apache_config_prefix'),
    '#description' => t(
      'Prefix for the apache config file of each new site. The config files will be created as "[prefix][machine_name].conf" and "[prefix][machine_name]-le-ssl.conf" (if ssl is enabled).'),
    '#required' => TRUE,
  );

  $form['apache']['apache_template'] = array(
    '#type' => 'textarea',
    '#title' => t('Apache configuration template'),
    '#default_value' => $config->get('apache_template'),
    '#description' => t('Template for Apache configuration. Use {MACHINE_NAME} and {DOMAIN} as variables.'),
    '#rows' => 15,
  );



  $form['apache']['create_log_directories'] = array(
    '#type' => 'checkbox',
    '#title' => t('Create log directories automatically'),
    '#default_value' => $config->get('create_log_directories'),
    '#description' => t('Automatically create log directories for each site. Requires apache_log_path to be set.'),
  );

  $form['apache']['apache_log_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Apache logs directory'),
    '#default_value' => $config->get('apache_log_path'),
    '#description' => t('Base directory for Apache log files (e.g., /var/log/apache2).'),
    '#states' => array(
      'visible' => array(
        ':input[name="create_log_directories"]' => array('checked' => TRUE),
      ),
    ),
  );

  $form['apache']['log_directory_template'] = array(
    '#type' => 'textfield',
    '#title' => t('Log directory template'),
    '#default_value' => $config->get('log_directory_template') ?: '{MACHINE_NAME}',
    '#description' => t('Template for log directory name. Use {MACHINE_NAME} as variable.'),
    '#states' => array(
      'visible' => array(
        ':input[name="create_log_directories"]' => array('checked' => TRUE),
      ),
    ),
  );

  $form['apache']['apache_config_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Virtual hosts path'),
    '#default_value' => $config->get('apache_config_path'),
    '#description' => t('Apache virtual hosts configuration directory. In Mac, create a directory under `/opt/homebrew/etc/httpd/` and set the full path here. In Ubuntu set it to `/etc/apache2/sites-available`.'),
    '#required' => FALSE,
  );

  $form['apache']['apache_ensite_command'] = array(
    '#type' => 'textfield',
    '#title' => t('Apache enable site command'),
    '#default_value' => $config->get('apache_ensite_command'),
    '#description' => t('Command to enable Apache virtual host. Use `sudo a2ensite` in Ubuntu. Leave empty for Mac.'),
    '#required' => FALSE,
  );

  $form['apache']['apache_dissite_command'] = array(
    '#type' => 'textfield',
    '#title' => t('Apache disable site command'),
    '#default_value' => $config->get('apache_dissite_command'),
    '#description' => t('Command to disable Apache virtual host. Use `sudo a2dissite` in Ubuntu. Leave empty for Mac.'),
    '#required' => FALSE,
  );

  $form['apache']['apache_reload_command'] = array(
    '#type' => 'textfield',
    '#title' => t('Apache reload command'),
    '#default_value' => $config->get('apache_reload_command'),
    '#description' => t('Command to reload Apache (add sudo in production server). In Mac, it cannot be `brew services restart` or `brew services reload`, as both commands restart Apache and lose PHP execution. Use `/opt/homebrew/opt/httpd/bin/apachectl graceful` for Mac and `sudo systemctl reload apache2` for Ubuntu'),
    '#required' => FALSE,
  );

  $form['ssl'] = array(
    '#type' => 'fieldset',
    '#title' => t('SSL configuration (Let\'s Encrypt)'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#group' => 'settings_tabs',
  );

  $form['ssl']['ssl_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable automatic SSL certificates'),
    '#default_value' => $config->get('ssl_enabled'),
    '#description' => t('Automatically create SSL certificates with Let\'s Encrypt for new sites.'),
  );

  $form['ssl']['apache_ssl_template'] = array(
    '#type' => 'textarea',
    '#title' => t('Apache SSL configuration template'),
    '#default_value' => $config->get('apache_ssl_template'),
    '#description' => t('Template for Apache SSL configuration. Use {MACHINE_NAME} and {DOMAIN} as variables.'),
    '#rows' => 15,
    '#states' => array(
      'visible' => array(
        ':input[name="ssl_enabled"]' => array('checked' => TRUE),
      ),
    ),
  );

  $form['ssl']['certbot_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Certbot path'),
    '#default_value' => $config->get('certbot_path'),
    '#description' => t('Full path to certbot executable. Normally `/usr/bin/certbot` in Ubuntu.'),
    '#states' => array(
      'visible' => array(
        ':input[name="ssl_enabled"]' => array('checked' => TRUE),
      ),
    ),
  );

  // not used right now, as we do not create SSL certificates for each site.
  // $form['ssl']['ssl_email'] = array(
  //   '#type' => 'email',
  //   '#title' => t('Let\'s Encrypt email'),
  //   '#default_value' => $config->get('ssl_email'),
  //   '#description' => t('Email address to register with Let\'s Encrypt. Not used right now.'),
  //   '#states' => array(
  //     'visible' => array(
  //       ':input[name="ssl_enabled"]' => array('checked' => TRUE),
  //     ),
  //   ),
  // );

  $form['notifications'] = array(
    '#type' => 'fieldset',
    '#title' => t('Email notifications'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#group' => 'settings_tabs',
  );

  $form['notifications']['email_notifications'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable email notifications'),
    '#default_value' => $config->get('email_notifications'),
    '#description' => t('Send emails when sites are created or errors occur.'),
  );

  $form['notifications']['notification_emails'] = array(
    '#type' => 'textarea',
    '#title' => t('Notification emails'),
    '#default_value' => $config->get('notification_emails'),
    '#description' => t('Comma-separated emails that will receive notifications.'),
    '#rows' => 3,
    '#states' => array(
      'visible' => array(
        ':input[name="email_notifications"]' => array('checked' => TRUE),
      ),
    ),
  );

  $form['notifications']['email_from'] = array(
    '#type' => 'email',
    '#title' => t('From email'),
    '#default_value' => $config->get('email_from'),
    '#description' => t('Email address that will appear as sender in notifications.'),
    '#states' => array(
      'visible' => array(
        ':input[name="email_notifications"]' => array('checked' => TRUE),
      ),
    ),
  );
  $form['notifications']['email_template'] = array(
    '#type' => 'textarea',
    '#title' => t('Email template'),
    '#default_value' => $config->get('email_template') ?: multisite_creator_default_email_template(),
    '#description' => t('Template for notification emails. Available variables: {SITE_NAME}, {MACHINE_NAME}, {URL}, {ADMIN_EMAIL}, {ADMIN_USERNAME}, {DATE}, {HOST}'),
    '#rows' => 10,
    '#states' => array(
      'visible' => array(
        ':input[name="email_notifications"]' => array('checked' => TRUE),
      ),
    ),
  );

  $form['debug'] = array(
    '#type' => 'fieldset',
    '#title' => t('Debug'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#group' => 'settings_tabs',
  );
  $form['debug']['debug'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable debugging info in logs'),
    '#default_value' => $config->get('debug'),
  );

  $form['cron'] = array(
    '#type' => 'fieldset',
    '#title' => t('Cron'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#group' => 'settings_tabs',
  );
  $form['cron']['max_batch'] = array(
    '#type' => 'number',
    '#title' => t('Max. batch'),
    '#default_value' => $config->get('max_batch')?: 5,
    '#description'  => t('Number of sites processed for each cron run.')
  );

  return system_settings_form($form);
}

/**
 * Validation for the configuration form.
 */
function multisite_creator_settings_form_validate($form, &$form_state) {
  $values = $form_state['values'];

  // Validate domain
  $domain = $values['base_domain'];
  if (!filter_var('http://' . $domain, FILTER_VALIDATE_URL)) {
    form_set_error('base_domain', t('The domain does not have a valid format.'));
  }

  // Validate SSL configuration
  if ($values['ssl_enabled']) {
    // if (empty($values['ssl_email'])) {
    //   form_set_error('ssl_email', t('You must specify an email for Let\'s Encrypt.'));
    // }

    $certbot_path = $values['certbot_path'];
    if (!file_exists($certbot_path) || !is_executable($certbot_path)) {
      form_set_error('certbot_path', t('The certbot path does not exist or is not executable.'));
    }
  }

  // Validate notification configuration
  if ($values['email_notifications']) {
    if (empty($values['notification_emails'])) {
      form_set_error('notification_emails', t('You must specify at least one email for notifications.'));
    } else {
      // Validate email formats
      $emails = preg_split('/[,\s]+/', trim($values['notification_emails']));
      foreach ($emails as $email) {
        if (!filter_var(trim($email), FILTER_VALIDATE_EMAIL)) {
          form_set_error('notification_emails', t('Email "@email" does not have a valid format.', array('@email' => $email)));
        }
      }
    }

    if (empty($values['email_from']) || !filter_var($values['email_from'], FILTER_VALIDATE_EMAIL)) {
      form_set_error('email_from', t('You must specify a valid sender email address.'));
    }
  }
}

/**
 * Gets the URL of a site.
 */
function multisite_creator_get_site_url($machine_name) {
  $config = config('multisite_creator.settings');
  $base_domain = $config->get('base_domain');
  return 'http://' . $machine_name . '.' . $base_domain;
}


/**
 * Page to manage existing multisite sites.
 */
function multisite_creator_manage_sites_page() {
  $sites = multisite_creator_get_existing_sites();

  if (empty($sites)) {
    return array(
      '#markup' => '<p>' . t('No multisite sites have been created yet. ') .
        l(t('Create the first site'), 'admin/config/system/multisite-creator') . '</p>',
    );
  }

  $header = array(
    t('Site name'),
    t('Machine name'),
    t('URL'),
    t('Status'),
    t('Creation date'),
    t('Actions'),
  );

  $rows = array();
  foreach ($sites as $machine_name => $site_info) {
    $site_url = multisite_creator_get_site_url($machine_name);

    // Determine site status
    $status = multisite_creator_get_site_status($machine_name);
    $status_class = $status['status'] === 'active' ? 'status-ok' : 'status-warning';

    $actions = array();
    $actions[] = l(t('Visit'), $site_url, array('attributes' => array('target' => '_blank')));
    $actions[] = l(
      t('Delete'),
      'admin/config/system/multisite-creator/delete/' . $machine_name,
      array('attributes' => array('class' => array('btn-danger')))
    );

    $rows[] = array(
      'data' => array(
        $site_info['name'] ?: $machine_name,
        $machine_name,
        l($site_url, $site_url, array('attributes' => array('target' => '_blank'))),
        array(
          'data' => $status['message'],
          'class' => array($status_class),
        ),
        $site_info['created'] ? format_date($site_info['created'], 'short') : t('Unknown'),
        implode(' | ', $actions),
      ),
    );
  }

  $build = array();

  $build['summary'] = array(
    '#markup' => '<p>' . t(
      'Total sites: <strong>@count</strong>',
      array('@count' => count($sites))
    ) . '</p>',
  );

  $build['sites_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#attributes' => array('class' => array('multisite-table')),
    '#empty' => t('No sites to show.'),
  );

  $build['actions'] = array(
    '#markup' => '<p>' .
      l(
        t('Create new site'),
        'admin/config/system/multisite-creator',
        array('attributes' => array('class' => array('button', 'button-primary')))
      ) .
      '</p>',
  );

  // Add custom CSS
  $build['#attached']['css'][] = array(
    'data' => '
      .multisite-table .status-ok { color: green; font-weight: bold; }
      .multisite-table .status-warning { color: orange; font-weight: bold; }
      .multisite-table .status-error { color: red; font-weight: bold; }
      .btn-danger:hover { background-color: #d9534f; color: white; padding: 0.3rem; }
      .btn-danger { background: transparent; color: #d9534f; padding: 0.3rem;}
    ',
    'type' => 'inline',
  );

  return $build;
}

/**
 * Gets the list of existing multisite sites.
 */
function multisite_creator_get_existing_sites() {
  $sites_dir = BACKDROP_ROOT . '/sites';
  $sites = array();

  if (!is_dir($sites_dir)) {
    return $sites;
  }

  $dirs = scandir($sites_dir);
  foreach ($dirs as $dir) {
    if ($dir === '.' || $dir === '..' || $dir === 'all' || $dir === 'default') {
      continue;
    }

    $full_path = $sites_dir . '/' . $dir;
    if (!is_dir($full_path)) {
      continue;
    }

    // Check if it has settings.php (is a valid site)
    $settings_file = $full_path . '/settings.php';
    if (!file_exists($settings_file)) {
      continue;
    }

    // Try to get site information
    $site_info = multisite_creator_get_site_info($dir);
    $sites[$dir] = $site_info;
  }

  return $sites;
}

/**
 * Gets information about a specific site.
 */
function multisite_creator_get_site_info($machine_name) {
  $sites_dir = BACKDROP_ROOT . '/sites/' . $machine_name;
  $settings_file = $sites_dir . '/settings.php';

  $info = array(
    'name' => '',
    'created' => NULL,
    'size' => 0,
  );

  // Get directory creation date
  if (is_dir($sites_dir)) {
    $info['created'] = filemtime($sites_dir);
    $info['size'] = multisite_creator_get_directory_size($sites_dir);
  }

  // Try to get site name from logs
  $query = db_select('watchdog', 'w');
  $query->fields('w', array('message', 'variables'));
  $query->condition('type', 'multisite_creator');
  $query->condition('message', 'New site created:%', 'LIKE');
  $query->orderBy('timestamp', 'DESC');
  $query->range(0, 10);
  $logs = $query->execute()->fetchAll();

  foreach ($logs as $log) {
    $variables = unserialize($log->variables);
    if (isset($variables['@site']) && strpos($log->message, $machine_name) !== FALSE) {
      $info['name'] = $variables['@site'];
      break;
    }
  }

  return $info;
}

/**
 * Gets the status of a multisite site.
 */
function multisite_creator_get_site_status($machine_name) {
  $site_url = multisite_creator_get_site_url($machine_name);

  // Check if the site responds
  $context = stream_context_create(array(
    'http' => array(
      'timeout' => 5,
      'method' => 'GET',
    )
  ));

  $response = @file_get_contents($site_url, false, $context);

  if ($response !== FALSE) {
    if (strpos($response, 'backdrop') !== FALSE || strpos($response, 'Backdrop') !== FALSE) {
      return array('status' => 'active', 'message' => t('Active'));
    } else {
      return array('status' => 'warning', 'message' => t('Responds but may have issues'));
    }
  } else {
    return array('status' => 'error', 'message' => t('Not responding'));
  }
}

/**
 * Form to delete a multisite site.
 */
function multisite_creator_delete_form($form, &$form_state, $machine_name) {
  $sites = multisite_creator_get_existing_sites();

  if (!isset($sites[$machine_name])) {
    backdrop_not_found();
    return;
  }

  $site_info = $sites[$machine_name];
  $site_url = multisite_creator_get_site_url($machine_name);

  $form['machine_name'] = array(
    '#type' => 'value',
    '#value' => $machine_name,
  );

  $form['site_info'] = array(
    '#markup' => '<div class="messages warning">' .
      '<h3>' . t('You are about to delete the site:') . '</h3>' .
      '<ul>' .
      '<li><strong>' . t('Name:') . '</strong> ' . ($site_info['name'] ?: $machine_name) . '</li>' .
      '<li><strong>' . t('URL:') . '</strong> ' . $site_url . '</li>' .
      '<li><strong>' . t('Directory:') . '</strong> sites/' . $machine_name . '</li>' .
      '</ul>' .
      '<p><strong>' . t('THIS ACTION CANNOT BE UNDONE!') . '</strong></p>' .
      '</div>',
  );

  $form['confirmation'] = array(
    '#type' => 'textfield',
    '#title' => t('Type the machine name "@machine_name" to confirm deletion', array('@machine_name' => $machine_name)),
    '#description' => t('Please type exactly: <strong>@machine_name</strong>', array('@machine_name' => $machine_name)),
    '#required' => TRUE,
    '#size' => 60,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Delete site permanently'),
    '#attributes' => array('class' => array('btn-danger')),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'admin/config/system/multisite-creator/manage'),
  );

  return $form;
}

/**
 * Validation for the deletion form.
 */
function multisite_creator_delete_form_validate($form, &$form_state) {
  $machine_name = $form_state['values']['machine_name'];
  $confirmation = $form_state['values']['confirmation'];

  if ($confirmation !== $machine_name) {
    form_set_error('confirmation', t('The machine name you entered does not match. Please type exactly: @machine_name', array('@machine_name' => $machine_name)));
  }
}

/**
 * Processing of the deletion form.
 */
function multisite_creator_delete_form_submit($form, &$form_state) {
  $machine_name = $form_state['values']['machine_name'];

  // Delete the site completely
  $success = multisite_creator_delete_site_completely($machine_name);

  if ($success) {
    backdrop_set_message(t(
      'Site @site has been deleted completely.',
      array('@site' => $machine_name)
    ));
    watchdog(
      'multisite_creator',
      'Site @site deleted manually',
      array('@site' => $machine_name),
      WATCHDOG_NOTICE
    );
  } else {
    backdrop_set_message(t('There were problems deleting the site. Check the logs.'), 'error');
  }

  $form_state['redirect'] = 'admin/config/system/multisite-creator/manage';
}

function multisite_creator_default_email_template() {
  $template = array();
  $template[] = 'A new multisite website has been created:';
  $template[] = '';
  $template[] = 'Site name: {SITE_NAME}';
  $template[] = 'Machine name: {MACHINE_NAME}';
  $template[] = 'URL: {URL}';
  $template[] = 'Administrator email: {ADMIN_EMAIL}';
  $template[] = 'Administrator username: {ADMIN_USERNAME}';
  $template[] = '';
  $template[] = 'Creation date: {DATE}';
  $template[] = 'Created from: {HOST}';
  $template[] = '';
  $template[] = '--';
  $template[] = 'This email was automatically generated by the Multisite Creator module.';

  return implode("\n", $template);
}
