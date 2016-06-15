var apiRoot = "https://api.github.com/";

// Display the stats
function showStats(data) {

    var err = false;
    var errMessage = '';

    if(data.status == 404) {
        err = true;
        errMessage = "The project does not exist!";
    }

    if(data.length == 0) {
        err = true;
        errMessage = "There are no releases for this project";
    }

    var html = '';

    if(err) {
        html = "<div>" + errMessage + "</div>";
    } else {
        html += "<div>";

	var releaseAssets = data.assets;
	var hasAssets = releaseAssets.length != 0;

	if(hasAssets) {
	    
	    $.each(releaseAssets, function(index, asset) {
		var assetSize = (asset.size / 1000000.0).toFixed(2);
		var lastUpdate = asset.updated_at.split("T")[0];
		html += "<p>" + asset.name + " (" + assetSize + "MB) - Downloaded " +
		    asset.download_count + " times.<br><i>Last updated on " + lastUpdate + "</i></p>";
	      
	    });
	    
	}
	html += "</div>";
    }

    var resultDiv = $("#stats-result");
    resultDiv.hide();
    resultDiv.html(html);
    $("#loader-gif").hide();
    resultDiv.slideDown();
}

// Callback function for getting release stats
function getStats() {
	var user = 'Sosoyan';
	var repository = 'Aton';

    var url = apiRoot + "repos/" + user + "/" + repository + "/releases/latest";
    $.getJSON(url, showStats).fail(showStats);
}
getStats();
