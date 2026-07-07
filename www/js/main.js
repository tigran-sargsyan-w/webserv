(function () {
  document.addEventListener("click", function (event) {
    var toggle = event.target.closest("[data-nav-toggle]");
    if (toggle) {
      var links = document.querySelector(".nav-links");
      if (links) links.classList.toggle("open");
    }
  });

  var current = window.location.pathname.replace(/\/$/, "") || "/index.html";
  if (current === "") current = "/index.html";
  document.querySelectorAll(".nav-links a").forEach(function (link) {
    var href = (link.getAttribute("href") || "").replace(/\/$/, "") || "/index.html";
    if (href === current) link.classList.add("active");
  });

  var deleteButton = document.querySelector("[data-delete-btn]");
  if (deleteButton) {
    deleteButton.addEventListener("click", function () {
      var input = document.querySelector("[data-delete-path]");
      var output = document.querySelector("[data-delete-result]");
      var path = input ? input.value.trim() : "";
      if (!path) {
        output.textContent = "Enter a resource path first.";
        return;
      }
      output.textContent = "Sending DELETE " + path + " ...";
      fetch(path, { method: "DELETE" })
        .then(function (response) {
          return response.text().then(function (body) {
            output.textContent = response.status + " " + response.statusText + (body ? "\n\n" + body : "");
          });
        })
        .catch(function (error) {
          output.textContent = "Network error: " + error;
        });
    });
  }
})();