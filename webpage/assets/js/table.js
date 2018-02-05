var table = d3.select("#gene_table");

var column_names = ["Gene ID"];
var headers = table.select("tr").selectAll("th")
    .data(column_names)
  .enter()
    .append("th")
    .text(function(d) { return d; });

d3.tsv("../assets/tsv/gene.tsv", function(data) {

    hideTableDiv();
    var keys = d3.keys(data[0]);

	var small_data = [];
    for(i in data) {
    	if(i < 50)
			small_data.push(data[i]);
        else
            break;
    }

    rows = table.select("tbody").selectAll("tr")
    .data(small_data).enter().append("tr")
    .on('click', function(d) {
    	rows.classed('selected', false);
    	d3.select(this).classed('selected', true);
		return tableRowClick(d);
	});

    rows.selectAll("td")
        .data(function(d) {
            return keys.map(function(k) {
                return { 'value': d[k], 'name': k};
            });
        }).enter().append("td")
        .attr('key', function (d) {
		    	return d.name;
		})
		.text(function (d) {
            return d.value;
        });

	d3.select("#search_button").on("click", function() {
        var searched_data = data;
        var text = document.getElementById("search").value;
        
        hideTableDiv();
        
        if(text.length > 0) {
			var searchResults = searched_data.map(function(r) {
				var geneId = Object.keys(r).map(function(key) {
					return r[key];
				});
			  	var regex = new RegExp(".*" + text + ".*", "i");
			  	if (regex.test(geneId)) {
					return regex.exec(geneId)[0];
			  	} 
			});

			searchResults = searchResults.filter(function(r) { 
			  return r != undefined;
			});

			searchResults = searchResults.filter(onlyUnique);

			searched_data = searchResults.map(function(r) {
			   return data.filter(function(p) {
			   	var geneId = Object.keys(p).map(function(key) {
					return p[key];
				});
				return geneId.indexOf(r) != -1;
			  })
			});

			searched_data = [].concat.apply([], searched_data);
        }

		var small_data = [];
    	for(i in searched_data) {
    		if(i < 50)
				small_data.push(searched_data[i]);
            else
                break;
    	}

        rows = table.select("tbody").selectAll("tr")
                .data(small_data).enter().append("tr")
                .on('click', function(d) {
                	rows.classed('selected', false);
    				d3.select(this).classed('selected', true);
                	return tableRowClick(d);
                });

        rows.selectAll("td")
            .data(function(d) {
                return keys.map(function(k) {
                    return { 'value': d[k], 'name': k};
                })
            }).enter().append("td")
            .attr('key', function (d) {
		    	return d.name;
		  })
		  .text(function (d) {
            return d.value;
	       });
	       showTableDiv(searched_data.length);
    });
    showTableDiv(data.length);
});

function tableRowClick(data) {
	document.getElementById("gene_id").value = Object.keys(data).map(function(key) {
		return data[key];
	});
    document.getElementById("plot_button").disabled = false;
}

function onlyUnique(value, index, self) { 
    return self.indexOf(value) === index;
}

function hideTableDiv() {
	document.getElementById('search_button').disabled = true;
	var tDiv = document.getElementById('t_div');
	tDiv.style.display = "none";
	document.getElementById('tbody').innerHTML = '';
	document.getElementById('plot_button').disabled = true;
}

function showTableDiv(dataLength) {
	var tDiv = document.getElementById('t_div');
	var mapMsg = document.getElementById('50_map_msg');
	document.getElementById('search_button').disabled = false;
    if(dataLength >= 50)
        mapMsg.innerHTML = 'Showing first 50 mappings';
    else
        mapMsg.innerHTML = 'Showing ' + dataLength + ' mappings';

	tDiv.style.display = "block";
	document.getElementById('plot_div').style.display = "none";
}
