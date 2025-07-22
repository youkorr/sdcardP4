import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_CS_PIN, CONF_ID
from esphome import pins

DEPENDENCIES = ["spi"]
AUTO_LOAD = []

sdcardP4_ns = cg.esphome_ns.namespace("sdcardP4")
SdcardP4Component = sdcardP4_ns.class_("SdcardP4Component", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(SdcardP4Component),
        cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    
    # Gestion robuste du CS pin
    cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs_pin))


