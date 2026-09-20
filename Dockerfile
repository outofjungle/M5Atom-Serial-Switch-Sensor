# ESP-IDF Docker Image for the M5Atom Serial Switch Sensor
#
# This project has no Matter dependency, so it uses the plain ESP-IDF image
# instead of the much larger espressif/esp-matter image used by sibling
# projects such as Matter-M5NanoC6-Switch.

FROM espressif/idf:v5.4.1

WORKDIR /project

CMD ["/bin/bash"]
