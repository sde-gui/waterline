template_config_DATA = \
	template/config
template_configdir = $(pkgdatadir)/profile/template

template_config_panel_DATA = \
	template/panels/panel.panel
template_config_paneldir = $(pkgdatadir)/profile/template/panels

template_applications_DATA = \
	template/applications/file-manager \
	template/applications/terminal-emulator \
	template/applications/volume-control
template_applicationsdir = $(pkgdatadir)/profile/template/applications

template_plugins_dclock_DATA = \
	template/plugins/dclock/formats
template_plugins_dclockdir = $(pkgdatadir)/profile/template/plugins/dclock/


uidir=$(pkgdatadir)/ui
ui_in_files= \
	ui/panel-pref.glade \
	ui/launchbar.glade \
	ui/netstatus.glade

ui_DATA = $(ui_in_files:.glade=.ui)

# Generate GtkBuilder UI files from Glade files
%.ui: %.glade
	cp $< $@
	$(top_builddir)/src/xml-purge $@

CLEANFILES = \
        $(ui_DATA)


lxpanelx_images_DATA = \
	images/my-computer.png \
	images/stock_volume.png \
	images/file-manager.png \
	images/gnome-logout.png \
	images/gnome-netstatus-0-24.png \
	images/gnome-netstatus-25-49.png \
	images/gnome-netstatus-50-74.png \
	images/gnome-netstatus-75-100.png \
	images/gnome-netstatus-disconn.png \
	images/gnome-netstatus-error.png \
	images/gnome-netstatus-idle.png \
	images/gnome-netstatus-rx.png \
	images/gnome-netstatus-txrx.png \
	images/gnome-netstatus-tx.png \
	images/background.png \
	images/volume.png \
	images/mute.png \
	images/ns-bothrs.png \
	images/ns-connected.png \
	images/ns-disconnect.png \
	images/ns-problem.png \
	images/ns-recvdata.png \
	images/ns-senddata.png \
	images/ns-lock.png \
	images/capslock-on.png \
	images/capslock-off.png \
	images/numlock-on.png \
	images/numlock-off.png \
	images/scrllock-on.png \
	images/scrllock-off.png \
	images/window-manager.png \
	images/clock.png \
	images/cpufreq-icon.png


lxpanelx_imagesdir = $(pkgdatadir)/images



EXTRA_DIST = \
	template/panels/panel.panel.in \
	$(lxpanelx_DATA) \
	$(lxpanelx_images_DATA) \
	$(ui_DATA) \
	$(template_config_DATA) \
	$(template_applications_DATA) \
	$(template_plugins_dclock_DATA)

