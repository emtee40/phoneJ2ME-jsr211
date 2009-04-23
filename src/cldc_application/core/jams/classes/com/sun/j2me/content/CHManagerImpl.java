/*
 *
 *
 * Copyright  1990-2009 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation. 
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt). 
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA 
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions. 
 */

package com.sun.j2me.content;

import javax.microedition.content.ContentHandlerException;
import javax.microedition.content.ContentHandlerServer;
import javax.microedition.content.Invocation;
import javax.microedition.content.Registry;

import com.sun.midp.security.SecurityInitializer;
import com.sun.midp.security.SecurityToken;
import com.sun.midp.security.ImplicitlyTrustedClass;

import javax.microedition.midlet.MIDlet;

import com.sun.midp.installer.InstallState;
import com.sun.midp.installer.Installer;
import com.sun.midp.installer.InvalidJadException;
import com.sun.midp.midlet.MIDletSuite;

/**
 * Handle all of the details of installing ContentHandlers.
 * Called at by the installer at the appropriate times to
 * {@link #preInstall parse and verify the JAD/Manifest attributes} and
 * {@link #install remove old content handlers and register new ones}.
 * If the installation fails the old handlers are
 * {@link #restore restored}.
 * When a suite is to be removed the content handlers are
 * {@link #uninstall uninstalled}.
 *
 *<p>
 * Two versions of this file exist; one which is no-op used when
 * MIDP stack is not built with CHAPI and the real implementation when
 * MIDP stack is BUILT with CHAPI.
 */
public class CHManagerImpl extends CHManagerBase {

    /**
     * Inner class to request security token from SecurityInitializer.
     * SecurityInitializer should be able to check this inner class name.
     */
    static private class SecurityTrusted implements ImplicitlyTrustedClass {};

    static {
        if( AppProxy.LOGGER != null ) AppProxy.LOGGER.println( "CHManagerImpl.<static initializer>" );
        SecurityToken classSecurityToken =
                SecurityInitializer.requestToken(new SecurityTrusted());
        com.sun.midp.content.CHManager.setCHManager(classSecurityToken, new CHManagerImpl());
        AppProxy.setSecurityToken(classSecurityToken);
        
        // load Invocation class
        Class cl = Invocation.class;
        cl = cl.getClass();
        if( AppProxy.LOGGER != null ) AppProxy.LOGGER.println( "Invocation class has loaded" );
    }

    /** Installed handlers accumulator. */
    private RegistryInstaller regInstaller;

    /** The Invocation in progress for an install. */
    private Invocation installInvoc;

    /** The ContentHandler for the Installer. */
    ContentHandlerServer handler;

    /**
     * Creates a new instance of CHManagerImpl.
     * Always initialize the Access to the Registry as if the
     * GraphicalInstaller is running.
     */
    private CHManagerImpl() {
        super();
        if( AppProxy.LOGGER != null ) AppProxy.LOGGER.println( "CHManagerImpl()" );
    }

    /**
     * Install the content handlers found and verified by preinstall.
     * Register any content handlers parsed from the JAD/Manifest
     * attributes.
     */
    public void install() {
        if( AppProxy.LOGGER != null ) AppProxy.LOGGER.println( "CHManagerImpl.install" );
        if (regInstaller != null) {
            regInstaller.install();
            regInstaller = null; // Let GC take it.
        }
    }

    /**
     * Parse the ContentHandler attributes and check for errors.
     * <ul>
     * <li> Parse attributes into set of ContentHandlers.
     * <li> If none, return
     * <li> Check for permission to install handlers
     * <li> Check each for simple invalid arguments
     * <li> Check each for MIDlet is registered
     * <li> Check each for conflicts with other application registrations
     * <li> Find any current registrations
     * <li> Merge current dynamic current registrations into set of new
     * <li> Check and resolve any conflicts between static and curr dynamic
     * <li> Retain current set and new set for registration step.
     * </ul>
     * @param installer the installer with access to the JAR, etc.
     * @param state the InstallState with the attributes and other context
     * @param msuite access to information about the suite
     * @param authority the authority, if any, that authorized the trust level
     * @exception InvalidJadException if there is no classname field,
     *   or if there are more than five comma separated fields on the line.
     */
    public void preInstall(Installer installer, InstallState state,
					MIDletSuite msuite, String authority) throws InvalidJadException
    {
        if( AppProxy.LOGGER != null ) 
        	AppProxy.LOGGER.println( "CHManagerImpl.preInstall(): installer = " + 
        			installer + ", state = " + state + ", msuite = " + msuite + 
        			"\n\tauthority = '" + authority + "'" );
		try {
		    AppBundleProxy bundle =
		    	new AppBundleProxy(installer, state, msuite, authority);
            regInstaller = new RegistryInstaller(bundle);
            regInstaller.preInstall();
		} catch (IllegalArgumentException ill) {
		    throw new InvalidJadException(
				  InvalidJadException.INVALID_CONTENT_HANDLER, ill.getMessage());
		} catch (ContentHandlerException che) {
		    if (che.getErrorCode() == ContentHandlerException.AMBIGUOUS) {
				throw new InvalidJadException(
					      InvalidJadException.CONTENT_HANDLER_CONFLICT,
					      che.getMessage());
		    } else {
				throw new InvalidJadException(
					      InvalidJadException.INVALID_CONTENT_HANDLER,
					      che.getMessage());
		    }
		} catch (ClassNotFoundException cnfe) {
		    throw new InvalidJadException(InvalidJadException.CORRUPT_JAR,
						  cnfe.getMessage());
		}
        if( AppProxy.LOGGER != null ) AppProxy.LOGGER.println( "CHManagerImpl.preInstall() exit" );
    }

    /**
     * Uninstall the Content handler specific information for
     * the specified suiteId.
     * @param suiteId the suiteId
     */
    public void uninstall(int suiteId) {
        if( AppProxy.LOGGER != null ) AppProxy.LOGGER.println( "CHManagerImpl.uninstall()" );
        RegistryInstaller.uninstallAll(suiteId, false);
    }


    /**
     * The content handler registrations are restored to the previous
     * state.
     */
    public void restore() {
    }

    /**
     * Get a URL to install from the Invocation mechanism, if one
     * has been queued.
     * @param midlet to check for an invocation
     * @return the URL to install; <code>null</code> if none available.
     * @see com.sun.midp.content.CHManagerImpl
     */
    public String getInstallURL(MIDlet midlet) {
		try {
		    handler = Registry.getServer(midlet.getClass().getName());
		} catch (ContentHandlerException che) {
	        return null;
	    }

        installInvoc = handler.getRequest(false);
        if (installInvoc != null) {
            String url = installInvoc.getURL();
            if (url != null && url.length() > 0) {
                return url;
            }
        }
        return null;
    }

    /**
     * Complete the installation of the URL provided by
     * {@link #getInstallURL} with the success/failure status
     * provided.
     * @param success <code>true</code> if the install was a success
     * @see com.sun.midp.content.CHManagerImpl
     */
    public void installDone(boolean success) {
        if( AppProxy.LOGGER != null ) AppProxy.LOGGER.println( "CHManagerImpl.installDone()" );
        if (installInvoc != null) {
		    handler.finish(installInvoc,
				   success ? Invocation.OK : Invocation.CANCELLED);
            installInvoc = null;
            regInstaller = null; // Double-clean.
        }
    }

}
