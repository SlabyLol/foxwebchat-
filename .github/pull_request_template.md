name: Theme Submission
description: Submit a new FoxWebChat theme
title: "[THEME] "
labels: ["theme-submission"]

body:
  - type: markdown
    attributes:
      value: |
        # Theme Submission
        
        Thank you for submitting a theme to FoxWebChat! Please fill out all fields completely.
        
        Note: It may take some time for your submission to be reviewed and processed.

  - type: textarea
    id: theme_name
    attributes:
      label: Theme Name
      description: What is the name of your theme?
      placeholder: "e.g. Dark Mode Pro, Neon Vibes, etc."
    validations:
      required: true

  - type: textarea
    id: theme_description
    attributes:
      label: Theme Description
      description: Describe your theme briefly
      placeholder: "Describe the color scheme, style and special features..."
    validations:
      required: true

  - type: textarea
    id: theme_file_link
    attributes:
      label: Theme File (.fwct)
      description: Upload your .fwct file to this PR and provide the link here
      placeholder: "Link to your .fwct file in this PR (e.g., themes/my_theme.fwct)"
    validations:
      required: true

  - type: textarea
    id: preview
    attributes:
      label: Preview / Screenshot
      description: Add a screenshot or preview of your theme
      placeholder: "Drag a screenshot here or paste a link"
    validations:
      required: false

  - type: dropdown
    id: category
    attributes:
      label: Theme Category
      description: Select the best category for your theme
      options:
        - Dark Mode
        - Light Mode
        - High Contrast
        - Colorful
        - Minimal
        - Custom
    validations:
      required: true

  - type: checkboxes
    id: ownership
    attributes:
      label: Ownership & Rights
      options:
        - label: I am the owner of the made .fwct file or I have allowance to submit this file
          required: true

  - type: textarea
    id: notes
    attributes:
      label: Additional Notes
      description: Any additional information about your theme?
      placeholder: "Special features, notes, etc."
    validations:
      required: false

Copyright DarkFox Co.
