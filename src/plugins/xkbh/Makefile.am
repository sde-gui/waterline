xkbh_la_CFLAGS = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	-DFLAGSDIR=\"$(datadir)/lxpanelx/images/xkbh-flags\" \
	-DXKBCONFDIR=\"$(datadir)/lxpanelx/xkeyboardconfig\" \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = xkbh.la
moduledir = $(pkglibdir)/plugins

xkbh_la_SOURCES = \
	xkbh-plugin.c \
	xkbh.c \
	xkbh.h

xkbh_la_LIBADD = \
	$(PACKAGE_LIBS)

xkbh_la_LDFLAGS = \
	-module \
	@LXPANELX_MODULE@

xkeyboardconfigdir=$(datadir)/lxpanelx/xkeyboardconfig
xkeyboardconfig_DATA=xkeyboardconfig/models.cfg \
	xkeyboardconfig/layouts.cfg \
	xkeyboardconfig/toggle.cfg

flagsdir=$(datadir)/lxpanelx/images/xkbh-flags
flags_DATA= \
	flags/ad.svg \
	flags/ae.svg \
	flags/af.svg \
	flags/al.svg \
	flags/am.svg \
	flags/ar.svg \
	flags/ara.svg \
	flags/at.svg \
	flags/az.svg \
	flags/ba.svg \
	flags/bd.svg \
	flags/be.svg \
	flags/ben.svg \
	flags/bg.svg \
	flags/bh.svg \
	flags/br.svg \
	flags/brai.svg \
	flags/bt.svg \
	flags/bw.svg \
	flags/by.svg \
	flags/ca.svg \
	flags/cd.svg \
	flags/ch.svg \
	flags/cm.svg \
	flags/cn.svg \
	flags/cu.svg \
	flags/cz.svg \
	flags/de.svg \
	flags/dev.svg \
	flags/dj.svg \
	flags/dk.svg \
	flags/dvorak.svg \
	flags/dz.svg \
	flags/ee.svg \
	flags/eg.svg \
	flags/epo.svg \
	flags/es.svg \
	flags/et.svg \
	flags/fi.svg \
	flags/fo.svg \
	flags/fr.svg \
	flags/gb.svg \
	flags/ge.svg \
	flags/gh.svg \
	flags/gn.svg \
	flags/gr.svg \
	flags/hr.svg \
	flags/hu.svg \
	flags/ie.svg \
	flags/il.svg \
	flags/in.svg \
	flags/iq.svg \
	flags/ir.svg \
	flags/is.svg \
	flags/it.svg \
	flags/jo.svg \
	flags/jp.svg \
	flags/ke.svg \
	flags/kg.svg \
	flags/kh.svg \
	flags/km.svg \
	flags/kp.svg \
	flags/kr.svg \
	flags/kw.svg \
	flags/kz.svg \
	flags/la.svg \
	flags/lb.svg \
	flags/lk.svg \
	flags/lt.svg \
	flags/lv.svg \
	flags/ly.svg \
	flags/ma.svg \
	flags/mao.svg \
	flags/me.svg \
	flags/mk.svg \
	flags/ml.svg \
	flags/mm.svg \
	flags/mn.svg \
	flags/mt.svg \
	flags/mv.svg \
	flags/mx.svg \
	flags/ng.svg \
	flags/nl.svg \
	flags/no.svg \
	flags/np.svg \
	flags/om.svg \
	flags/ph.svg \
	flags/pk.svg \
	flags/pl.svg \
	flags/pt.svg \
	flags/qa.svg \
	flags/ro.svg \
	flags/rs.svg \
	flags/ru.svg \
	flags/sa.svg \
	flags/sd.svg \
	flags/se.svg \
	flags/si.svg \
	flags/sk.svg \
	flags/sn.svg \
	flags/so.svg \
	flags/sr.svg \
	flags/sy.svg \
	flags/th.svg \
	flags/tj.svg \
	flags/tm.svg \
	flags/tn.svg \
	flags/tr.svg \
	flags/tw.svg \
	flags/tz.svg \
	flags/ua.svg \
	flags/us.svg \
	flags/uy.svg \
	flags/uz.svg \
	flags/vn.svg \
	flags/ye.svg \
	flags/za.svg

EXTRA_DIST = \
	$(flags_DATA) \
	$(xkeyboardconfig_DATA)
