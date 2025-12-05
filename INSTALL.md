# Install Instructions for Multisite Creator for Backdrop CMS

## Requirements

This module requires full control of the server where this Backdrop multisite
installation runs. It will not work on a server you manage through cPanel or
any other type of server management platform.

This module has been tested and works on systems with the following
characteristics:

### Development Server

- macOS Sequoia 15.7
- Apache 2 and MySQL running locally and managed by Homebrew (this module will
  not work with Lando or DDEV)
- We do not serve HTTPS pages locally.

A Linux computer can be used as a local development environment. Follow the
instructions for the production server to configure it.

### Production Server

- Ubuntu 24.04 server
- Apache 2
- PHP (or PHP-FPM)
- MySQL
- Certbot for SSL certificate management

## Initial Configuration

This module creates directories and files under the `sites` directory of your
Backdrop installation, as well as all the necessary Apache config files and SSL
certificates (if necessary). Your server, then, needs to be set up so the user
Apache is running as has all the permissions to perform those tasks.

### Development Server Configuration

#### Apache, PHP and MySQL

Install httpd, PHP and MySQL with Homebrew. There is good documentation on
this topic here: https://getgrav.org/blog/macos-sequoia-apache-multiple-php-versions

Once properly installed, create a directory under `/opt/homebrew/etc/httpd/extra/`
to store the Apache config files. This will be the *Virtual hosts path* to set
in the Multisite Creator configuration form.

Decide where you will host your websites. `/Users/<your_username>/Sites/` is a
good location. Set *DocumentRoot* in `/opt/homebrew/etc/httpd/httpd.conf` to that
path.

There should be no permission issues on this development server, as httpd
should run as your user and should have permissions to perform all the tasks this
module executes to create a new site.

If you do not want to use the root user of **MySQL**, make sure to create a new
user with at least *CREATE* and *DROP* permissions on any database or the ones
with the prefix you set in the *Database prefix* field of the config form of
this module.

### Production Server Configuration

Do a standard installation and configuration of Apache, PHP, MySQL and Certbot
on the server.

#### Apache

**Apache** should run as `www-data` and you need to give this user
permissions to perform the tasks this module needs to execute. For this reason, you
need to add `www-data` to the sudoers file with these values:

```bash
www-data ALL=(ALL) NOPASSWD: /usr/sbin/a2ensite *
www-data ALL=(ALL) NOPASSWD: /usr/sbin/a2dissite *
www-data ALL=(ALL) NOPASSWD: /usr/bin/systemctl reload apache2*
www-data ALL=(ALL) NOPASSWD: /usr/bin/rm /etc/apache2/sites-available/msc_*
www-data ALL=(ALL) NOPASSWD: /usr/bin/rm -rf /_apache_log_dir_/*
www-data ALL=(ALL) NOPASSWD: /usr/bin/cp /tmp/apache_*.conf /etc/apache2/sites-available/*.conf
www-data ALL=(ALL) NOPASSWD: /usr/bin/certbot *
```

This line:

```bash
www-data ALL=(ALL) NOPASSWD: /usr/bin/rm /etc/apache2/sites-available/msc_*
```

...contains a prefix (`msc_`) set in the [*Apache config file prefix*](#apache-config-file-prefix) value of
the Multisite Creator config form. You can use a different prefix, of course.
Just make sure you use the same prefix in both places. This prefix is not
necessary, but using it adds some security as the `www-data` user will not
be able to delete any Apache config files other than the ones managed by this
module.

Also, note this line:

```bash
www-data ALL=(ALL) NOPASSWD: /usr/bin/rm -rf /_apache_log_dir_/*
```

...which allows the user www-data to delete any config directory inside the Apache
log dir you will set in the configuration form of this module.

The rest of the paths in that sudoers file are the defaults for a standard
installation of Apache and Certbot on Ubuntu 24.04. If your paths are different,
change them accordingly.

#### MySQL

For **MySQL**, it is highly recommended to create a special user to manage
your site's databases. Just make sure to create it with at least *CREATE* and
*DROP* permissions on any database or the ones with the prefix you set in the
*Database prefix* field of the config form of this module. This would be an
example of the commands to run to create that user. Change the password and user
name to your liking:

```bash
-- Create user with permissions to create databases
CREATE USER 'backdrop_creator'@'localhost' IDENTIFIED BY 'secure_password';
GRANT CREATE, DROP ON *.* TO 'backdrop_creator'@'localhost';
GRANT ALL PRIVILEGES ON `site_%`.* TO 'backdrop_creator'@'localhost';
FLUSH PRIVILEGES;
```

#### Certbot

To configure **Certbot** for this module, it is necessary to create a single
certificate for all the subdomains of your Backdrop multisite installation. This
is the command that will create it:

```bash
sudo certbot certonly --manual --preferred-challenges dns -d "*._your_domain_.com" -d "_your_domain_.com"
```

As you can see, this command will install a certificate for any subdomain of your
domain, as well as one certificate for your main domain. Certbot will provide you
with a TXT record you need to add to the DNS server of your domain.
Without this TXT record entry, your Backdrop multisite installation WILL NOT
be able to serve HTTPS pages.

## Install Your Main Backdrop Site

Install this module in the usual way for a Backdrop module and enable it.

### Settings.php Configurations

The main site's settings.php file allows the following configurations, which
will apply to all websites generated with the module:

- $settings['msc_db_pass']: database password (required)
  - This is the password of the user who will create and drop the databases for
    each site
- $settings['msc_user1_pass']: user 1 password (optional)
- $settings['msc_user1_name']: user 1 username (optional)
- $settings['msc_user1_email']: user 1 email (optional)
  - These are the credentials for user 1 of each site you create. Do not set them
    if you want to set different credentials for each site.

Optional configurations not added in settings.php can be configured in the
website creation form.

#### Important

This modules needs a `default.settings.php` file you need to put in `sites/deafult/`
which will act as a templates for new subsites. You can use the main `settings.php`
which comes with a new backdrop or download it from
https://raw.githubusercontent.com/backdrop/backdrop/refs/heads/1.x/settings.php

### Configure Multisite Creator Module

Go to `admin/config/system/multisite-creator/settings` to configure the module.

#### Database Server

Set here the credentials to access your database server. The password should be
set in settings.php.

#### Apache Configuration

##### Base Domain

This is the main domain from which you are serving your multisite Backdrop.
Every new site created with this module will be a subdomain of this main domain.

##### Apache Config File Prefix

This is the prefix for all Apache config files created for any site.

##### Apache Configuration Template

This is the template used to create the Apache config file for every site. The
tokens `{MACHINE_NAME}` and `{DOMAIN}` will be substituted by the site's machine
name set in the creation form and the base domain set in this same form.

This is an example of a template for the development environment on a Mac:

```bash
<VirtualHost *:80>
  DocumentRoot "/Users/_your_user_/Sites/_your_site_"
  ServerName {DOMAIN}
  <Directory /Users/_your_user_/Sites/_your_site_/>
    Options Indexes FollowSymLinks
    AllowOverride All
    Require all granted
  </Directory>
  ErrorLog "/Users/_your_user_/Sites/httpd_logs/{MACHINE_NAME}/error.log"
  CustomLog "/Users/_your_user_/Sites/httpd_logs/{MACHINE_NAME}/access.log" common
</VirtualHost>
```

If you want separate log files for each site, you need to add them here and
configure the following section.

This is an example for a production site on Ubuntu with PHP-FPM:

```bash
<VirtualHost *:80>
  ServerAdmin _your_email_
  ServerName {DOMAIN}
  DocumentRoot /home/_your_user_/_your_site_
  <Directory /home/_your_user_/_your_site_/>
      Options Indexes FollowSymLinks
      AllowOverride All
      Require all granted
  </Directory>
  CustomLog "|/usr/bin/rotatelogs -n 3 /home/_your_user_/logs/{MACHINE_NAME}/access.log 5M" common
  ErrorLog "|/usr/bin/rotatelogs -n 3 /home/_your_user_/logs/{MACHINE_NAME}/error.log 50K"
  <FilesMatch \.php$>
    # Apache 2.4.10+ can proxy to unix socket
    SetHandler "proxy:unix:/run/php/php8.3-fpm.sock|fcgi://localhost"
  </FilesMatch>
</VirtualHost>
```

##### Create Log Directories Automatically

Check this box if you set the log files for each site in the Apache configuration
template. New fields will be shown.

##### Apache Logs Directory

This is the directory where the logs will be kept. It has to be the same directory
that you set in the template. As in the last example, it should be set to:

`/home/_your_user_/logs`

##### Log Directory Template

This is set, by default, to `{MACHINE_NAME}`, but you could add a prefix, for
instance. Remember, though, that it has to be the same value that you set in the
template above.

##### Virtual Hosts Path

Apache virtual hosts configuration directory. On Mac, create a directory under
`/opt/homebrew/etc/httpd/` and set the full path here. On Ubuntu set it to
`/etc/apache2/sites-available`.

##### Apache Enable Site Command

Command to enable Apache virtual host. Use `sudo a2ensite` on Ubuntu. Leave
empty for Mac.

##### Apache Disable Site Command

Command to disable Apache virtual host. Use `sudo a2dissite` on Ubuntu. Leave
empty for Mac.

##### Apache Reload Command

Command to reload Apache (add sudo on production server). On Mac, it cannot be
`brew services restart` or `brew services reload`, as both commands restart
Apache and lose PHP execution. Use `/opt/homebrew/opt/httpd/bin/apachectl graceful`
for Mac and `sudo systemctl reload apache2` for Ubuntu.

#### SSL Configuration

Use SSL configuration only for Ubuntu servers. This is not working yet for Mac.

##### Enable Automatic SSL Certificates

Check this box if you want to enable SSL configuration on your Ubuntu server.

##### Apache SSL Configuration Template

This is the template used by this module to create the SSL config files for
Apache. We need to create them, even though Certbot can add them automatically, because
we have experienced some connection errors with Let's Encrypt servers when
creating queued sites. This way, we only need one certificate for the main site
which covers all the subdomains.

This is an example template file for Ubuntu:

```bash
<IfModule mod_ssl.c>
  <VirtualHost *:443>
    ServerAdmin _your_email_
    ServerName {DOMAIN}
    DocumentRoot /home/_your_user_/_your_site_
    <Directory /home/_your_user_/_your_site_/>
        Options Indexes FollowSymLinks
        AllowOverride All
        Require all granted
    </Directory>
    CustomLog "|/usr/bin/rotatelogs -n 3 /home/_your_user_/logs/{MACHINE_NAME}/access.log 5M" common
    ErrorLog "|/usr/bin/rotatelogs -n 3 /home/_your_user_/logs/{MACHINE_NAME}/error.log 50K"

    Include /etc/letsencrypt/options-ssl-apache.conf
    SSLCertificateFile /etc/letsencrypt/live/_your_site_.com/fullchain.pem
    SSLCertificateKeyFile /etc/letsencrypt/live/_your_site_.com/privkey.pem
  </VirtualHost>
</IfModule>
```

If using independent log files, like in this example, remember to set the log
values in the Apache configuration section above.

##### Certbot Path

Full path to certbot executable. Normally `/usr/bin/certbot` on Ubuntu.

#### Email Notifications

This module sends an email to the addresses set here when a new site is created
or there has been an error creating one.

##### Enable Email Notifications

Check this box to enable email notifications. New form elements will appear.

##### Notification Emails

Set all the email addresses which should receive these notifications.

##### From Email

`From email` value for sent emails.

##### Email Template

Template for notification emails. Available variables: {SITE_NAME},
{MACHINE_NAME}, {URL}, {ADMIN_EMAIL}, {ADMIN_USERNAME}, {DATE}, {HOST}

Example of email template:

```txt
A new multisite website has been created:

Site name: {SITE_NAME}
Machine name: {MACHINE_NAME}
URL: {URL}
Administrator email: {ADMIN_EMAIL}
Administrator username: {ADMIN_USERNAME}

Creation date: {DATE}
Created from: {HOST}
```

#### Debug

##### Enable Debugging Info in Logs

Check this box to enable the debugging info in watchdog.

#### Cron

All the sites created through the creation form are put in a queue which will be
cleared at cron runs.

##### Max. Batch

Set the maximum number of sites created at each cron run.

## Create Your First Multisite

1. Go to `Admin > Config > System > Multisite Creator`.
2. Fill out the form with:
   - **Site name**: The title that will appear on the website.
   - **Profile**: The profile to install your site with.
   - **Administrator email**: Email for the new site's administrator user.
   - **Username**: Username for the administrator.
   - **Password**: Password for the administrator.
3. Click "Create site".
4. Run cron.

The module will automatically create:

- Directory `sites/machine_name/`
- Configured `settings.php` file
- Configured `sites.php` file
- Database with defined prefix
- Complete Backdrop installation
- Apache Virtual Host configuration
- SSL certificate with Let's Encrypt (if enabled)
- User 1 with default data
- Email notification to administrators (if enabled)

## Developers

This module adds a couple of hooks and has some other functionality which can
be used by developers in their own projects, if they want to interact with this
module.

### API

There is a function `hook_multisite_creator_form_alter()` and a function
`hook_multisite_creator_form_validate()` available to developers to add their own
form elements in the `Create new site` form when choosing their own custom
profile. The most basic usage is to add a second user created for each site.

These two hooks need to be added in your `.profile` file of your custom profile.

There are also other hooks which need to be added in your `.install` file of your
custom profile. These hooks are provided by core: `hook_install_tasks()`,
`hook_config_form()` and `hook_config_form_submit()`.

See `multisite_creator.api.php` file for an example of implementation of these
hooks to add a second user account in your newly created site.

### Create a New Site from Your Module

You can also create a site from within your code by calling one of these two
functions: `multisite_creator_create_site($site_data);` or `multisite_creator_add_to_queue($site_data);`.

The first function creates a site straight away. The second one adds a new site
to the queue. Multisite creator module creates all the sites in the queue at each
cron run to avoid concurrency.

This is the array you have to pass to both functions:

```php
$site_data = array(
  'name' => '_name_of_the_site_',
  'machine_name' => '_machine_name_',
  'profile' => 'standard',
  'user1_email' => 'mail@example.com',
  'user1_name' => '_your_username_',
  'user1_pass' => '_your_password_',
  'extra_fields' => array(),
);
```

You have to take into account that there cannot be two sites with the same machine
name. This module checks for duplicated sites before calling those two functions.
Thus, you need to check for duplicates before calling any of those creation
functions. You can use `multisite_creator_site_exists($machine_name)` to check for
duplicates.
