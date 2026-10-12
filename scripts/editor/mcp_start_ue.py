"""Starts the MCP server of this editor: when two editors started together, the second one could not take the port."""
import unreal as ue

ue.SystemLibrary.execute_console_command(None, "ModelContextProtocol.StartServer")
ue.log("MCP server start requested")
