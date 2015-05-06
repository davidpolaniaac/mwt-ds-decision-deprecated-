﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace JoinServerUploader
{
    /// <summary>
    /// Represents the method that will handle the <see cref="JoinServerUploader.PackageSent"/>
    /// event of a <see cref="JoinServerUploader"/> object.
    /// </summary>
    /// <param name="sender">The source of the event.</param>
    /// <param name="e">A <see cref="PackageEventArgs"/> object that contains the event data.</param>
    public delegate void PackageSentEventHandler(object sender, PackageEventArgs e);

    /// <summary>
    /// Represents the method that will handle the <see cref="JoinServerUploader.PackageSendFailed"/>
    /// event of a <see cref="JoinServerUploader"/> object.
    /// </summary>
    /// <param name="sender">The source of the event.</param>
    /// <param name="e">A <see cref="PackageEventArgs"/> object that contains the event data.</param>
    public delegate void PackageSendFailedEventHandler(object sender, PackageEventArgs e);

    /// <summary>
    /// Provides data for the <see cref="JoinServerUploader.PackageSent"/> and <see cref="JoinServerUploader.PackageSendFailed"/>
    /// events.
    /// </summary>
    public class PackageEventArgs : EventArgs
    {
        /// <summary>
        /// Records that were included in the event.
        /// </summary>
        public IEnumerable<string> Records { get; set; }

        /// <summary>
        /// The identifier of the package that was sent.
        /// </summary>
        public Guid PackageId { get; set; }
    }
}
