<?php
/**
 * @file
 * Hooks provided by the Multisite Creator module.
 */

/**
 * @addtogroup hooks
 * @{
 */

/**
 * Allows profiles to add specific fields to the site creation form.
 *
 * This hook allows each installation profile to add its own fields to the
 * form for creating new multisite sites. Fields added will be sent as
 * arguments during site installation.
 *
 * IMPORTANT: For this hook to work, the installation profile's .profile file
 * must define this function. The module will automatically load the .profile
 * file of the selected profile.
 *
 * @param array &$form
 *   The form container where profile-specific fields should be added.
 *   This is a subarray of $form['profile_extra_fields'].
 * @param array &$form_state
 *   The current form state.
 *
 *   Field naming convention:
 *   - Field names must follow the pattern: [form_name]__[field_name]
 *   - form_name: Name of the profile form where data will be sent
 *   - field_name: Name of the field within the profile form
 *   - Example: 'my_profile_config_form__extra_user_name'
 *
 *   During installation, these fields will be converted to arguments:
 *   my_profile_config_form.extra_user_name=value.
 *
 * @see install.sh
 * @see hook_install_tasks()
 */
function hook_multisite_creator_form_alter(array &$form, array &$form_state) {
  $form['extra_config'] = array(
    '#type' => 'fieldset',
    '#title' => t('Extra configuration for my profile'),
    '#collapsible' => FALSE,
  );

  // Field to create an extra user
  // Will be converted to: my_profile_config_form.extra_user_name=value.
  $form['extra_config']['my_profile_config_form__extra_user_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Extra username'),
    '#maxlength' => 60,
    '#description' => t('Extra username that will be created in the new site.'),
  );

  $form['extra_config']['my_profile_config_form__extra_user_email'] = array(
    '#type' => 'email',
    '#title' => t('Extra user email'),
    '#description' => t('Email for the extra site user.'),
  );

  $form['extra_config']['my_profile_config_form__extra_user_pass'] = array(
    '#type' => 'password',
    '#title' => t('Extra user password'),
    '#description' => t('Password for the extra site user.'),
  );

  // Example with multiple forms.
  $form['extra_config']['my_profile_settings_form__site_slogan'] = array(
    '#type' => 'textfield',
    '#title' => t('Site slogan'),
    '#description' => t('Slogan that will appear on the site.'),
  );
}

/**
 * Example of profile-specific validation for fields.
 *
 * @param array $form
 *   The complete form.
 * @param array &$form_state
 *   The form state.
 */
function hook_multisite_creator_form_validate(array $form, array &$form_state) {
  $values = $form_state['values'];

  // Validate that extra username has at least 3 characters.
  if (!empty($values['my_profile_config_form__extra_user_name'])) {
    if (strlen($values['my_profile_config_form__extra_user_name']) < 3) {
      form_set_error(
        'my_profile_config_form__extra_user_name',
        t('Extra username must be at least 3 characters long.')
      );
    }
  }

  // Validate that if username is provided, email is also provided.
  $user_name = $values['my_profile_config_form__extra_user_name'];
  $user_email = $values['my_profile_config_form__extra_user_email'];

  if (!empty($user_name) && empty($user_email)) {
    form_set_error(
      'my_profile_config_form__extra_user_email',
      t('You must specify an email for the extra user.')
    );
  }
}

/**
 * Example implementation in the profile's .install file.
 *
 * This code shows how to receive and process arguments sent from the
 * multisite creation form.
 */
function hook_install_tasks($install_state) {
  $tasks = array();

  // Define custom task to process extra fields.
  $tasks['my_profile_config_form'] = array(
    'display_name' => t('Extra profile configuration'),
    'type' => 'form',
    'run' => INSTALL_TASK_RUN_IF_NOT_COMPLETED,
  );

  return $tasks;
}

/**
 * Example implementation in the profile's .install file.
 *
 * Form to process arguments from multisite creator.
 */
function hook_config_form($form, &$form_state, &$install_state) {
  // Hidden fields to receive values from arguments.
  $form['extra_user_name'] = array(
    '#type' => 'hidden',
    '#default_value' => '',
  );

  $form['extra_user_email'] = array(
    '#type' => 'hidden',
    '#default_value' => '',
  );

  $form['extra_user_pass'] = array(
    '#type' => 'hidden',
    '#default_value' => '',
  );

  return $form;
}

/**
 * Example implementation in the profile's .install file.
 *
 * Submit handler for the profile's custom form.
 */
function hook_config_form_submit($form, &$form_state) {
  $values = $form_state['values'];

  // Create extra user if data is provided.
  if (!empty($values['extra_user_name']) && !empty($values['extra_user_email'])) {
    $account = entity_create('user', array());
    $account->name = $values['extra_user_name'];
    $account->mail = $values['extra_user_email'];
    $account->pass = $values['extra_user_pass'] ?: user_password();
    $account->status = 1;
    $account->roles = array(BACKDROP_AUTHENTICATED_ROLE);

    $account->save();

    if ($account->uid) {
      backdrop_set_message(t(
        'Extra user @user created successfully.',
        array('@user' => $values['extra_user_name'])
      ));
    }
  }
}

/**
 * @} End of "addtogroup hooks".
 */
