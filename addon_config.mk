# addon_config.mk for ofxEbbControl

#----------------------------------------------------------------
# This file tells the OpenFrameworks project generator about
# the ofxEbbControl addon location and include paths.
#----------------------------------------------------------------

# Name of this addon folder
ADDON_NAME = ofxEbbControl

# Root OF directory (inherited from parent project)
# OF_ROOT variable is set by OF project generator

# Include path for headers
# Adjust if your headers live under src/ or include/
USER_INC += -I$(OF_ROOT)/addons/$(ADDON_NAME)

# Link against any additional libraries here (none needed)
# USER_LIBS += -L/path/to/lib -lname

# If you need to copy resources, list them below
# RESOURCE_DIRS += data

# End of addon configuration

